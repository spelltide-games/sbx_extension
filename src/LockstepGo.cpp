#include "LockstepGo.hpp"
#include "MessagePack.hpp"
#include "godot_cpp/classes/time.hpp"

namespace pkpy {

LockstepGoClient::LockstepGoClient(String host, int port) {
	this->host = host;
	this->port = port;
	this->current_step = 0;

	methods["set_id"] = [](LockstepGoClient *client, Variant arg) -> Variant {
		if (client->id.is_empty()) {
			client->id = arg;
		}
		return {};
	};

	methods["update_room"] = [](LockstepGoClient *client, Variant arg) -> Variant {
		client->room = arg;
		return {};
	};

	methods["request_game_state"] = [](LockstepGoClient *client, Variant arg) -> Variant {
		return client->call("_export_game_state");
	};

	this->is_rpc_pending = false;
}

void LockstepGoClient::poll() {
	switch (current_step) {
		case 0: {
			// connect_ws
			String ws_url = "ws://" + host + ":" + String::num(port);
			Error err = ws_peer.connect_to_url(ws_url);
			if (err == OK) {
				current_step++;
				print_line("lockstep-go: connected to " + ws_url);
			}
			break;
		}
		case 1: {
			// wait for id
			if (!id.is_empty()) {
				current_step++;
				print_line("lockstep-go: received id " + id);
			}
			break;
		}
		case 2: {
			// wait for room
			if (!room.is_empty()) {
				// connect_kcp
				int udp_port = room["port"];
				Error err = kcp.udp_peer.connect_to_host(host, udp_port);
				if (err == OK) {
					kcp.ikcp = ikcp_create(udp_port, this);
					kcp.ikcp->output = [](const char *buf, int len, ikcpcb *kcp, void *user) -> int {
						LockstepGoClient *client = (LockstepGoClient *)user;
						PackedByteArray data;
						data.resize(len);
						std::memcpy(data.ptrw(), buf, len);
						client->kcp.udp_peer.put_packet(data);
						return len;
					};
					ikcp_nodelay(kcp.ikcp, 1, 20, 2, 1);
					ikcp_setmtu(kcp.ikcp, 900);
					ikcp_wndsize(kcp.ikcp, 128, 128);
					current_step++;
					print_line("lockstep-go: connected to UDP port " + String::num(udp_port));
				}
			}
			break;
		}
		default: {
			poll_ws();
			poll_kcp();
			break;
		}
	}
}

void LockstepGoClient::poll_kcp() {
	// update
	ikcp_update(kcp.ikcp, Time::get_singleton()->get_ticks_msec());
	// input
	while (kcp.udp_peer.get_available_packet_count() > 0) {
		PackedByteArray packet = kcp.udp_peer.get_packet();
		ikcp_input(kcp.ikcp, (const char *)packet.ptr(), packet.size());
	}
	// recv
	char buffer[2048];
	int recv_len = ikcp_recv(kcp.ikcp, buffer, sizeof(buffer));
	PackedByteArray packet;
	packet.resize(recv_len);
	std::memcpy(packet.ptrw(), buffer, recv_len);
	// parse
	Array cpnts = MessagePack::loads(packet);
	String srcID = cpnts[0];
	int cmd = cpnts[1];
	Variant arg = cpnts[2];
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
			// [frame_id, inputs]
			call("_on_client_frame", arr[0], arr[1]);
			break;
		}
	}
}

void LockstepGoClient::poll_ws() {
	ws_peer.poll();
	int state = ws_peer.get_ready_state();
	switch (state) {
		case WebSocketPeer::STATE_CONNECTING:
			break;
		case WebSocketPeer::STATE_OPEN:
			while (ws_peer.get_available_packet_count() > 0) {
				PackedByteArray packet = ws_peer.get_packet();
				Array arr = MessagePack::loads(packet);

				if (arr.size() != 4) {
					print_error("lockstep-go: invalid ws message");
					continue;
				}

				String srcID = arr[0];
				// String dstID = arr[1];
				String method = arr[2];
				Variant arg = arr[3];

				bool noReply = method.begins_with("!");
				if (noReply) {
					method = method.substr(1);
				}

				if (method == "_") {
					if (is_rpc_pending) {
						rpc_result = arg;
						is_rpc_pending = false;
					} else {
						print_error("lockstep-go: received response but no pending call");
					}
				} else if (methods.has(method)) {
					Variant retval = methods[method](this, arg);
					if (!noReply) {
						PackedByteArray resp = MessagePack::dumps(Array::make(id, srcID, "_", retval));
						Error err = ws_peer.send(resp);
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

bool LockstepGoClient::rpc_call(String src_id, String dst_id, String method, Variant arg) {
	if (id.is_empty()) {
		print_error("lockstep-go: client_id is not set");
		return false;
	}
	if (is_rpc_pending) {
		print_error("lockstep-go: previous call is still pending");
		return false;
	}
	PackedByteArray data = MessagePack::dumps(Array::make(src_id, dst_id, method, arg));
	Error err = ws_peer.send(data);
	if (err != OK) {
		print_error("lockstep-go: ws_peer.send() failed");
		return false;
	}
	rpc_result.clear();
	is_rpc_pending = true;
	return true;
}

void LockstepGoClient::send_kcp(OpCodeKCP cmd, Variant arg) {
	PackedByteArray data = MessagePack::dumps(Array::make((int)cmd, arg));
	ikcp_send(kcp.ikcp, (const char *)data.ptr(), data.size());
}

void LockstepGoClient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("poll"), &LockstepGoClient::poll);
	ClassDB::bind_method(D_METHOD("rpc_call", "src_id", "dst_id", "method", "arg"), &LockstepGoClient::rpc_call);
	ClassDB::bind_method(D_METHOD("is_rpc_done"), &LockstepGoClient::is_rpc_done);
	ClassDB::bind_method(D_METHOD("get_rpc_result"), &LockstepGoClient::get_rpc_result);
	ClassDB::bind_method(D_METHOD("send_input", "arg"), &LockstepGoClient::send_input);
	ClassDB::bind_method(D_METHOD("create_room", "name", "version", "max_players", "frame_rate"), &LockstepGoClient::create_room);
	ClassDB::bind_method(D_METHOD("join_room", "port", "version", "pre_join"), &LockstepGoClient::join_room);
	ClassDB::bind_method(D_METHOD("leave_room"), &LockstepGoClient::leave_room);
	ClassDB::bind_method(D_METHOD("request_game_state"), &LockstepGoClient::request_game_state);
	// virtual methods
	ClassDB::bind_method(D_METHOD("_export_game_state"), &LockstepGoClient::_export_game_state);
	ClassDB::bind_method(D_METHOD("_on_client_frame", "frame_id", "inputs"), &LockstepGoClient::_on_client_frame);
}

} // namespace pkpy