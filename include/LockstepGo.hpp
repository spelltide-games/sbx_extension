#pragma once

#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/packet_peer_udp.hpp"
#include "godot_cpp/classes/web_socket_peer.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "ikcp.h"

namespace pkpy {

using namespace godot;

enum class OpCodeKCP : int {
	OpPing = 0,
	OpPong,
	OpServerInput,
	OpClientFrame,
};

#define IKCP_MTU_SIZE 900
#define IKCP_MAX_MESSAGE_SIZE 4096

class LockstepGoClient;
using LockstepGoMethod = Variant (*)(LockstepGoClient *client, Variant arg);

class LockstepGoClient : public Node {
	GDCLASS(LockstepGoClient, Node)

	Ref<WebSocketPeer> ws_peer;
	Ref<PacketPeerUDP> udp_peer;
	ikcpcb *ikcp;

	HashMap<String, LockstepGoMethod> methods;
	bool is_rpc_pending;

	String id;
	Dictionary room;
	int poll_stage;
	bool any_kcp_msg_received;

	String host;
	int port;

	LockstepGoClient();

	String get_id() const {
		return id;
	}

	Dictionary get_room() const {
		return room;
	}

	Signal connect_ws(String host, int port) {
		this->host = host;
		this->port = port;
		return Signal(this, "ws_connected");
	}

	Signal connect_room(Dictionary room) {
		this->room = room;
		return Signal(this, "room_connected");
	}

	void poll();
	void poll_kcp();
	void poll_ws();

	Signal rpc_call(String dst_id, String method, Variant arg);

	void send_kcp(OpCodeKCP cmd, Variant arg);
	void send_input(Variant arg) {
		send_kcp(OpCodeKCP::OpServerInput, arg);
	}

	Signal create_room(String version, int max_players, int frame_rate) {
		Dictionary arg;
		arg["version"] = version;
		arg["max_players"] = max_players;
		arg["frame_rate"] = frame_rate;
		return rpc_call("", "create_room", arg);
	}

	Signal join_room(int port, String version, bool pre_join) {
		Dictionary arg;
		arg["port"] = port;
		arg["version"] = version;
		arg["pre_join"] = pre_join;
		return rpc_call("", "join_room", arg);
	}

	void leave_room() {
		rpc_call("", "!leave_room", {});
	}

	void request_game_state() {
		Dictionary players = room["players"];
		for (Variant player : players.values()) {
			Dictionary p = player;
			if (p["id"] != id) {
				rpc_call(p["id"], "request_game_state", {});
			}
		}
		print_error("unreachable");
	}

	virtual ~LockstepGoClient() {
		if (ikcp) {
			ikcp_release(ikcp);
			ikcp = nullptr;
		}
		if (ws_peer.is_valid()) {
			ws_peer->close();
		}
	}

protected:
	static void _bind_methods();
};

} // namespace pkpy