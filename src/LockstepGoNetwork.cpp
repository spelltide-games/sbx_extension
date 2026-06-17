#include "godot_cpp/classes/packet_peer_udp.hpp"
#include "godot_cpp/classes/time.hpp"
#include "godot_cpp/classes/web_socket_peer.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"
#include "ikcp.h"
#include "pocketpy.h"

namespace pkpy {

using namespace godot;

#define IKCP_MAX_MESSAGE_SIZE 4096

static PackedByteArray bytes_to_gd(py_Ref bytes) {
	assert(bytes->type == tp_bytes);
	PackedByteArray buf;
	int size;
	unsigned char *src = py_tobytes(bytes, &size);
	buf.resize(size);
	std::memcpy(buf.ptrw(), src, size);
	return buf;
}

struct LockstepGoNetwork {
	Ref<WebSocketPeer> ws_peer;
	Ref<PacketPeerUDP> udp_peer;
	ikcpcb *ikcp;
	int udp_redundancy;
	bool ws_opened, ws_closed, kcp_opened;
	py_Name on_ws_data;
	py_Name on_kcp_data;
	py_Ref callbacks;

	LockstepGoNetwork(py_Ref callbacks) {
		ws_peer.instantiate();
		udp_peer.instantiate();
		ikcp = nullptr;
		udp_redundancy = 1;
		ws_opened = ws_closed = kcp_opened = false;
		on_ws_data = py_name("on_ws_data");
		on_kcp_data = py_name("on_kcp_data");
		this->callbacks = callbacks;
	}

	void dispose() {
		ws_peer->close();
		udp_peer->close();
		if (ikcp) {
			ikcp_release(ikcp);
			ikcp = nullptr;
		}
	}

	bool call_data(py_Name name, void *p, int size) {
		py_push(callbacks);
		if (!py_pushmethod(name)) {
			return AttributeError(callbacks, name);
		}
		py_Ref tmp = py_pushtmp();
		void *dst = py_newbytes(tmp, size);
		std::memcpy(dst, p, size);
		return py_vectorcall(1, 0);
	}

	~LockstepGoNetwork() {
		dispose();
	}
};

void setup_lockstepgo_module() {
	py_GlobalRef mod = py_newmodule("lockstepgo");
	py_Type t = py_newtype("LockstepGoNetwork", tp_object, mod, [](void *ud) {
		LockstepGoNetwork *self = (LockstepGoNetwork *)ud;
		self->~LockstepGoNetwork();
	});

	py_bindmethod(t, "connect_ws", [](int argc, py_Ref argv) {
		// connect_ws(self, host: str, port: int) -> int
		LockstepGoNetwork *self = (LockstepGoNetwork *)py_touserdata(py_arg(0));
		PY_CHECK_ARGC(3);
		PY_CHECK_ARG_TYPE(1, tp_str);
		PY_CHECK_ARG_TYPE(2, tp_int);
		const char *host = py_tostr(py_arg(1));
		int port = py_toint(py_arg(2));
		char url[512];
		snprintf(url, sizeof(url), "ws://%s:%d", host, port);
		Error err = self->ws_peer->connect_to_url(url);
		py_newint(py_retval(), err);
		return true;
	});

	py_bindmethod(t, "connect_kcp", [](int argc, py_Ref argv) {
		// connect_kcp(self, host: str, port: int, conv: int, udp_redundancy: int) -> int
		LockstepGoNetwork *self = (LockstepGoNetwork *)py_touserdata(py_arg(0));
		PY_CHECK_ARGC(5);
		PY_CHECK_ARG_TYPE(1, tp_str);
		PY_CHECK_ARG_TYPE(2, tp_int);
		PY_CHECK_ARG_TYPE(3, tp_int);
		PY_CHECK_ARG_TYPE(4, tp_int);
		const char *host = py_tostr(py_arg(1));
		int port = py_toint(py_arg(2));
		uint32_t conv = py_toint(py_arg(3));
		self->udp_redundancy = py_toint(py_arg(4));
		Error err = self->udp_peer->connect_to_host(host, port);
		if (err != OK) {
			py_newint(py_retval(), err);
			return true;
		}
		self->ikcp = ikcp_create(conv, self);
		self->ikcp->output = [](const char *buf, int len, ikcpcb *kcp, void *user) -> int {
			LockstepGoNetwork *net = (LockstepGoNetwork *)user;
			PackedByteArray data;
			data.resize(len);
			std::memcpy(data.ptrw(), buf, len);
			for (int i = 0; i < net->udp_redundancy; i++) {
				Error err = net->udp_peer->put_packet(data);
				if (err != OK) {
					print_error("LockstepGoNetwork: udp_peer.put_packet() failed: " + String::num_int64(err));
				}
			}
			return len;
		};
		ikcp_nodelay(self->ikcp, 1, 20, 2, 1);
		ikcp_setmtu(self->ikcp, 900);
		ikcp_wndsize(self->ikcp, 128, 128);
		py_newint(py_retval(), OK);
		return true;
	});

	py_bindmethod(t, "send_ws", [](int argc, py_Ref argv) {
		// send_ws(self, data: bytes) -> int
		LockstepGoNetwork *self = (LockstepGoNetwork *)py_touserdata(py_arg(0));
		PY_CHECK_ARGC(2);
		PY_CHECK_ARG_TYPE(1, tp_bytes);
		Error err = self->ws_peer->send(bytes_to_gd(py_arg(1)));
		py_newint(py_retval(), err);
		return true;
	});

	py_bindmethod(t, "send_kcp", [](int argc, py_Ref argv) {
		// send_kcp(self, data: bytes) -> int
		LockstepGoNetwork *self = (LockstepGoNetwork *)py_touserdata(py_arg(0));
		PY_CHECK_ARGC(2);
		PY_CHECK_ARG_TYPE(1, tp_bytes);
		int size;
		void *data = py_tobytes(py_arg(1), &size);
		int n = ikcp_send(self->ikcp, (const char *)data, size);
		if (n < 0) {
			print_error("LockstepGoNetwork: ikcp_send() failed: " + String::num_int64(n));
		}
		py_newint(py_retval(), n);
		return true;
	});

	py_bindmethod(t, "poll", [](int argc, py_Ref argv) {
		// poll(self) -> int
		LockstepGoNetwork *self = (LockstepGoNetwork *)py_touserdata(py_arg(0));
		PY_CHECK_ARGC(1);

		// poll ws
		self->ws_peer->poll();
		int state = self->ws_peer->get_ready_state();
		switch (state) {
			case WebSocketPeer::STATE_OPEN:
				if (!self->ws_opened) {
					self->ws_opened = true;
				}
				while (self->ws_peer->get_available_packet_count() > 0) {
					PackedByteArray packet = self->ws_peer->get_packet();
					if (!self->call_data(self->on_ws_data, (void *)packet.ptr(), packet.size())) {
						return false;
					}
				}
				break;
			case WebSocketPeer::STATE_CLOSED:
				if (!self->ws_closed) {
					self->ws_closed = true;
				}
				break;
		}

		// poll kcp
		if (self->ikcp) {
			// update
			ikcp_update(self->ikcp, Time::get_singleton()->get_ticks_msec());
			// input
			while (self->udp_peer->get_available_packet_count() > 0) {
				PackedByteArray packet = self->udp_peer->get_packet();
				int ec = ikcp_input(self->ikcp, (const char *)packet.ptr(), packet.size());
				if (ec != 0) {
					print_error("LockstepGoNetwork: ikcp_input() failed: " + String::num_int64(ec));
				}
			}

			// recv
			char recv_buf[IKCP_MAX_MESSAGE_SIZE];
			while (true) {
				int n = ikcp_recv(self->ikcp, recv_buf, sizeof(recv_buf));
				if (n > 0) {
					if (!self->kcp_opened) {
						self->kcp_opened = true;
					}
					// parse
					if (!self->call_data(self->on_kcp_data, (void *)recv_buf, n)) {
						return false;
					}
				} else {
					if (n == -1) {
						break; // no more message
					} else {
						print_error("LockstepGoNetwork: ikcp_recv() failed: " + String::num_int64(n));
						break;
					}
				}
			}
		}
		py_newnone(py_retval());
		return true;
	});

	py_bindmethod(t, "__new__", [](int argc, py_Ref argv) {
		// __new__(cls) -> LockstepGoNetwork
		void *ud = py_newobject(py_retval(), py_totype(py_arg(0)), 1, sizeof(LockstepGoNetwork));
		new (ud) LockstepGoNetwork(py_getslot(py_retval(), 0));
		return true;
	});

	py_bindmethod(t, "__init__", [](int argc, py_Ref argv) {
		// __init__(self, callbacks) -> LockstepGoNetwork
		PY_CHECK_ARGC(2);
		LockstepGoNetwork *self = (LockstepGoNetwork *)py_touserdata(py_arg(0));
		py_assign(self->callbacks, py_arg(1));
		py_newnone(py_retval());
		return true;
	});

	py_bindmethod(t, "dispose", [](int argc, py_Ref argv) {
		// dispose(self) -> None
		LockstepGoNetwork *self = (LockstepGoNetwork *)py_touserdata(py_arg(0));
		PY_CHECK_ARGC(1);
		self->dispose();
		py_newnone(py_retval());
		return true;
	});

	py_bindproperty(t, "ws_opened", [](int argc, py_Ref argv) {
            PY_CHECK_ARGC(1);
            LockstepGoNetwork *self = (LockstepGoNetwork *)py_touserdata(py_arg(0));
            py_newbool(py_retval(), self->ws_opened);
            return true; }, nullptr);

	py_bindproperty(t, "ws_closed", [](int argc, py_Ref argv) {
            PY_CHECK_ARGC(1);
            LockstepGoNetwork *self = (LockstepGoNetwork *)py_touserdata(py_arg(0));
            py_newbool(py_retval(), self->ws_closed);
            return true; }, nullptr);

	py_bindproperty(t, "kcp_opened", [](int argc, py_Ref argv) {
            PY_CHECK_ARGC(1);
            LockstepGoNetwork *self = (LockstepGoNetwork *)py_touserdata(py_arg(0));
            py_newbool(py_retval(), self->kcp_opened);
            return true; }, nullptr);
}

} //namespace pkpy
