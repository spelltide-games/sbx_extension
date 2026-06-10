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

struct LockstepKCP {
	PacketPeerUDP udp_peer;
	ikcpcb *ikcp;

	LockstepKCP() : ikcp(nullptr) {}
};

class LockstepGoClient;
using LockstepGoMethod = Variant (*)(LockstepGoClient *client, Variant arg);

class LockstepGoClient : public Node {
	GDCLASS(LockstepGoClient, Node)

	WebSocketPeer ws_peer;
	LockstepKCP kcp;

	HashMap<String, LockstepGoMethod> methods;
	Variant rpc_result;
	bool is_rpc_pending;

	String id;
	Dictionary room;

	String host;
	int port;
	int current_step;

	LockstepGoClient(String host, int port);

	void poll();
	void poll_kcp();
	void poll_ws();

	bool rpc_call(String srcID, String dstID, String method, Variant arg);

	bool is_rpc_done() const {
		return !is_rpc_pending;
	}

	Variant get_rpc_result() const {
		return rpc_result;
	}

	void send_kcp(OpCodeKCP cmd, Variant arg);
	void send_input(Variant arg) {
		send_kcp(OpCodeKCP::OpServerInput, arg);
	}

	bool create_room(String name, String version, int max_players, int frame_rate) {
		Dictionary arg;
		arg["name"] = name;
		arg["version"] = version;
		arg["max_players"] = max_players;
		arg["frame_rate"] = frame_rate;
		return rpc_call(id, "", "create_room", arg);
	}

	bool join_room(int port, String version, bool pre_join) {
		Dictionary arg;
		arg["port"] = port;
		arg["version"] = version;
		arg["pre_join"] = pre_join;
		return rpc_call(id, "", "join_room", arg);
	}

	bool leave_room() {
		return rpc_call(id, "", "!leave_room", {});
	}

	bool request_game_state() {
		Dictionary players = room["players"];
		for (Variant player : players.values()) {
			Dictionary p = player;
			if (p["id"] != id) {
				return rpc_call(id, p["id"], "request_game_state", {});
			}
		}
		// unreachable
		return false;
	}

	virtual Variant _export_game_state() {
		return {};
	}

	virtual void _on_client_frame(int frame_id, Dictionary inputs) {
		print_line("Received frame " + String::num(frame_id) + " with inputs length=" + String::num(inputs.size()));
	}

	virtual ~LockstepGoClient() {
		if (kcp.ikcp) {
			ikcp_release(kcp.ikcp);
			kcp.ikcp = nullptr;
		}
		ws_peer.close();
	}

protected:
	static void _bind_methods();
};

} // namespace pkpy