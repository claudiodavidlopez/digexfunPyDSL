"""External electronic load simulator for the co-simulation example.

Run with `python eload.py` before starting the PowerFactory simulation.
It answers each request [V] from script.py with [P, Q], and a
{"reset": true} request (sent by script.py at the start of every
simulation run) by starting the load model over, so one run's voltage dip
does not carry over into the next. Requires pyzmq.
"""

import zmq


class ElectronicLoadWECC:
    """Electronic load component of the WECC composite load model (CMPLDW).

    Same model as examples/electronic_load/script.py.
    """

    def __init__(self, Vd1, Vd2, frcel, Pel0, Qel0, Vmin0=1.0):
        self.Vd1 = Vd1
        self.Vd2 = Vd2
        self.frcel = frcel
        self.Pel0 = Pel0
        self.Qel0 = Qel0
        self.Vmin = Vmin0

    def calc_pq(self, V):
        if V < self.Vmin:
            self.Vmin = V

        if self.Vmin < self.Vd2:
            self.Vmin = self.Vd2

        if V < self.Vd2:
            Fvl = 0.0
        elif V < self.Vd1:
            if V <= self.Vmin:
                Fvl = (V - self.Vd2) / (self.Vd1 - self.Vd2)
            else:
                Fvl = (self.Vmin - self.Vd2 + self.frcel * (V - self.Vmin)) / (
                    self.Vd1 - self.Vd2
                )
        else:
            if self.Vmin >= self.Vd1:
                Fvl = 1.0
            else:
                Fvl = (self.Vmin - self.Vd2 + self.frcel * (self.Vd1 - self.Vmin)) / (
                    self.Vd1 - self.Vd2
                )

        return Fvl * self.Pel0, Fvl * self.Qel0


def new_load():
    return ElectronicLoadWECC(Vd1=0.866, Vd2=0.7, frcel=0.7, Pel0=20, Qel0=10)


def main():
    eload = new_load()

    context = zmq.Context()
    socket = context.socket(zmq.REP)
    socket.bind("tcp://*:7000")
    print("Electronic load listening on port 7000")

    while True:
        request = socket.recv_json()
        if isinstance(request, dict) and request.get("reset"):
            eload = new_load()
            socket.send_json({"reset": True})
            continue
        V = request[0]
        P, Q = eload.calc_pq(V)
        socket.send_json([P, Q])


if __name__ == "__main__":
    main()
