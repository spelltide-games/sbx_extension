#include "LockstepGo.hpp"
#include "MessagePack.hpp"
#include "godot_cpp/classes/time.hpp"
#include "godot_cpp/core/property_info.hpp"

namespace pkpy {

LockstepGoClient::LockstepGoClient() {
	this->ws_peer.instantiate();
	this->udp_peer.instantiate();
	this->ikcp = nullptr;

	this->poll_stage = 0;
	this->any_kcp_msg_received = false;

	this->host = "";
	this->port = 0;

	methods["set_id"] = [](LockstepGoClient *client, Variant arg) -> Variant {
		if (client->id.is_empty()) {
			client->id = arg;
		}
		return {};
	};

	methods["room_event"] = [](LockstepGoClient *client, Variant arg) -> Variant {
		print_line("Received room event: " + String(arg));
		return {};
	};

	methods["request_game_state"] = [](LockstepGoClient *client, Variant arg) -> Variant {
		return client->call("_export_game_state");
	};
}

void LockstepGoClient::poll() {
	if (host.is_empty() || port == 0) {
		return;
	}

	switch (poll_stage) {
		case 0: {
			// connect_ws
			String ws_url = "ws://" + host + ":" + String::num_int64(port);
			ws_peer->set_heartbeat_interval(20.0);
			Error err = ws_peer->connect_to_url(ws_url);
			if (err == OK) {
				poll_stage++;
			} else {
				poll_stage = -1;
			}
			break;
		}
		case 1: {
			// wait for id
			if (!id.is_empty()) {
				poll_stage++;
				emit_signal("ws_connected");
			}
			break;
		}
		case 2: {
			// wait for room
			if (!room.is_empty()) {
				// find conv
				Dictionary players = room["players"];
				int udp_port = room["port"];
				uint32_t conv = 0;
				for (Dictionary player : players.values()) {
					if (player["id"] == id) {
						conv = player["conv"];
					}
				}
				if (conv == 0) {
					print_error("lockstep-go: conv not found");
					poll_stage = -1;
					break;
				}
				// connect kcp
				Error err = udp_peer->connect_to_host(host, udp_port);
				if (err == OK) {
					ikcp = ikcp_create(conv, this);
					ikcp->output = [](const char *buf, int len, ikcpcb *kcp, void *user) -> int {
						LockstepGoClient *client = (LockstepGoClient *)user;
						PackedByteArray data;
						data.resize(len);
						std::memcpy(data.ptrw(), buf, len);
						client->udp_peer->put_packet(data);
						return len;
					};
					ikcp_nodelay(ikcp, 1, 20, 2, 1);
					ikcp_setmtu(ikcp, 900);
					ikcp_wndsize(ikcp, 128, 128);
					poll_stage++;
				} else {
					poll_stage = -1;
				}
			}
			break;
		}
		case 3: {
			// kcp ping
			send_kcp(OpCodeKCP::OpPing, {});
			poll_stage++;
			break;
		}
		case 4: {
			// wait for any kcp msg
			if (any_kcp_msg_received) {
				emit_signal("room_connected");
				poll_stage++;
			}
		}
		default: {
			break;
		}
	}

	poll_ws();

	if (ikcp) {
		poll_kcp();
	}
}

void LockstepGoClient::poll_kcp() {
	// print_line(Time::get_singleton()->get_ticks_msec());
	// update
	ikcp_update(ikcp, Time::get_singleton()->get_ticks_msec());
	// input
	while (udp_peer->get_available_packet_count() > 0) {
		PackedByteArray packet = udp_peer->get_packet();
		ikcp_input(ikcp, (const char *)packet.ptr(), packet.size());
	}
	// recv
	char recv_buf[IKCP_MAX_MESSAGE_SIZE];
	int recv_len = ikcp_recv(ikcp, recv_buf, sizeof(recv_buf));
	if (recv_len > 0) {
		// parse
		Array cpnts = MessagePack::loads_c(recv_buf, recv_len);
		if (cpnts.size() != 2) {
			print_error("lockstep-go: invalid kcp message");
			return;
		}
		int cmd = cpnts[0];
		Variant arg = cpnts[1];
		any_kcp_msg_received = true;
		switch ((OpCodeKCP)cmd) {
			case OpCodeKCP::OpPing: {
				send_kcp(OpCodeKCP::OpPong, arg);
				break;
			}
			case OpCodeKCP::OpPong: {
				break;
			}
			case OpCodeKCP::OpServerInput: {
				break;
			}
			case OpCodeKCP::OpClientFrame: {
				Array arr = arg;
				// [frame_id, inputs, ...]
				callv("_on_client_frame", arr);
				break;
			}
		}
	}
}

void LockstepGoClient::poll_ws() {
	ws_peer->poll();
	int state = ws_peer->get_ready_state();
	switch (state) {
		case WebSocketPeer::STATE_CONNECTING:
			break;
		case WebSocketPeer::STATE_OPEN:
			while (ws_peer->get_available_packet_count() > 0) {
				PackedByteArray packet = ws_peer->get_packet();
				Array arr = MessagePack::loads(packet);

				if (arr.size() != 4) {
					print_error("lockstep-go: invalid ws message");
					continue;
				}

				String src_id = arr[0];
				// String dst_id = arr[1];
				String method = arr[2];
				Variant arg = arr[3];

				bool no_reply = method.begins_with("!");
				if (no_reply) {
					method = method.substr(1);
				}

				if (method == "_") {
					if (is_rpc_pending) {
						emit_signal("rpc_done", arg);
						is_rpc_pending = false;
					} else {
						print_error("lockstep-go: received response but no pending call");
					}
				} else if (methods.has(method)) {
					Variant retval = methods[method](this, arg);
					if (!no_reply) {
						PackedByteArray resp = MessagePack::dumps(Array::make(id, src_id, "_", retval));
						Error err = ws_peer->send(resp);
						if (err != OK) {
							print_error("lockstep-go: ws_peer.send() failed");
						}
					}
				}
			}
			break;
		case WebSocketPeer::STATE_CLOSING:
			break;
		case WebSocketPeer::STATE_CLOSED:
			break;
	}
}

Signal LockstepGoClient::rpc_call(String dst_id, String method, Variant arg) {
	if (id.is_empty()) {
		print_error("lockstep-go: client_id is not set");
		return Signal(this, "unreachable");
	}
	if (is_rpc_pending) {
		print_error("lockstep-go: previous call is still pending");
		return Signal(this, "unreachable");
	}
	PackedByteArray data = MessagePack::dumps(Array::make(id, dst_id, method, arg));
	Error err = ws_peer->send(data);
	if (err != OK) {
		print_error("lockstep-go: ws_peer.send() failed");
		return Signal(this, "unreachable");
	}
	is_rpc_pending = true;
	return Signal(this, "rpc_done");
}

void LockstepGoClient::send_kcp(OpCodeKCP cmd, Variant arg) {
	PackedByteArray data = MessagePack::dumps(Array::make((int)cmd, arg));
	ikcp_send(ikcp, (const char *)data.ptr(), data.size());
}

void LockstepGoClient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("connect_ws", "host", "port"), &LockstepGoClient::connect_ws);
	ClassDB::bind_method(D_METHOD("connect_room", "room"), &LockstepGoClient::connect_room);
	ClassDB::bind_method(D_METHOD("poll"), &LockstepGoClient::poll);

	ClassDB::bind_method(D_METHOD("rpc_call", "dst_id", "method", "arg"), &LockstepGoClient::rpc_call);
	ClassDB::bind_method(D_METHOD("send_input", "arg"), &LockstepGoClient::send_input);
	ClassDB::bind_method(D_METHOD("create_room", "version", "max_players", "frame_rate"), &LockstepGoClient::create_room);
	ClassDB::bind_method(D_METHOD("join_room", "port", "version", "pre_join"), &LockstepGoClient::join_room);
	ClassDB::bind_method(D_METHOD("leave_room"), &LockstepGoClient::leave_room);
	ClassDB::bind_method(D_METHOD("request_game_state"), &LockstepGoClient::request_game_state);
	// virtual methods
	MethodInfo export_game_state_mi;
	export_game_state_mi.return_val = PropertyInfo(Variant::NIL, "", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT);
	export_game_state_mi.name = "_export_game_state";
	ClassDB::add_virtual_method(get_class_static(), export_game_state_mi);
	MethodInfo on_client_frame_mi;
	on_client_frame_mi.name = "_on_client_frame";
	on_client_frame_mi.arguments.push_back(PropertyInfo(Variant::INT, "frame_id"));
	on_client_frame_mi.arguments.push_back(PropertyInfo(Variant::DICTIONARY, "inputs"));
	on_client_frame_mi.arguments_metadata.push_back({});
	on_client_frame_mi.arguments_metadata.push_back({});
	ClassDB::add_virtual_method(get_class_static(), on_client_frame_mi, { "frame_id", "inputs" });
	// properties
	ClassDB::bind_method(D_METHOD("_get_id"), &LockstepGoClient::get_id);
	ClassDB::bind_method(D_METHOD("_get_room"), &LockstepGoClient::get_room);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "_get_id");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "room", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "_get_room");
	// signal
	ADD_SIGNAL(MethodInfo("ws_connected"));
	ADD_SIGNAL(MethodInfo("room_connected"));
	ADD_SIGNAL(MethodInfo("rpc_done", PropertyInfo(Variant::NIL, "result", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ADD_SIGNAL(MethodInfo("unreachable"));
}

} // namespace pkpy