import zmq


ADDRESS = 'tcp://localhost:7000'


class CosimInterface:
    def __init__(self, address):
        self.address = address
        self.context = None
        self.socket = None

    def _open_connection(self):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REQ)
        self.socket.connect(self.address)

    def _close_connection(self):
        self.socket.close()
        self.context.term()

    def exchange_variables(self, *sim_outs):
        self._open_connection()
        self.socket.send_json(sim_outs)
        sim_ins = self.socket.recv_json()
        self._close_connection()
        return tuple(sim_ins)


cs_int = CosimInterface(ADDRESS)


CALLABLE_REGISTRY = [
    cs_int.exchange_variables
]
