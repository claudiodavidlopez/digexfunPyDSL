"""Co-simulation interface for PowerFactory (paper, Section IV-B).

Every time step, the DSL model sends its outputs (here, the voltage at bus
B6) to an external simulator over ZeroMQ and receives that simulator's
outputs (here, the load's P and Q). Start eload.py first; it plays the
external simulator. Requires pyzmq (pip install pyzmq).

exchange_variables accepts any number of values: declare how many with
LoadPyFun(argNum, pyFunID).

Messages are JSON: a list of numbers to exchange variables, or
{"reset": true} to put the external simulator back in its initial state.
The DLL imports this script afresh at the start of every simulation run,
so the reset below is sent once per run. If the simulator does not answer
within TIMEOUT_MS, a TimeoutError is raised; the DLL prints it and stops
the simulation instead of PowerFactory waiting forever.
"""

import zmq

ADDRESS = "tcp://localhost:7000"
TIMEOUT_MS = 5000


class CosimInterface:
    def __init__(self, address, timeout_ms=TIMEOUT_MS):
        self.address = address
        self.timeout_ms = timeout_ms

    def _request(self, message):
        # A fresh connection per request, as in the paper. A REQ socket
        # cannot be reused after a timed-out request, so this also keeps
        # error handling simple.
        context = zmq.Context()
        socket = context.socket(zmq.REQ)
        socket.setsockopt(zmq.LINGER, 0)  # don't block on close if unsent
        socket.setsockopt(zmq.RCVTIMEO, self.timeout_ms)
        try:
            socket.connect(self.address)
            socket.send_json(message)
            return socket.recv_json()
        except zmq.Again:
            raise TimeoutError(
                f"no reply from {self.address} within {self.timeout_ms} ms; "
                "is eload.py running?"
            ) from None
        finally:
            socket.close()
            context.term()

    def reset(self):
        self._request({"reset": True})

    def exchange_variables(self, *sim_outs):
        return self._request(list(sim_outs))


cs_int = CosimInterface(ADDRESS)
cs_int.reset()  # new simulation run: start the external model from scratch

CALLABLE_REGISTRY = [
    cs_int.exchange_variables,  # pyFunID = 0
]
