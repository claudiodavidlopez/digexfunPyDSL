"""WECC electronic load model implemented in Python (paper, Section IV-A).

The Load Model block in WECC_Electronic_Load.pfd calls eload.calc_pq every
time step with the terminal voltage V (p.u.) and uses the returned active
and reactive power (MW, Mvar):

    pyFunID = 0
    inc(dummy0) = LoadPyFun(1, pyFunID)
    dummy1 = SetPyFunArg(V, 0, pyFunID)
    dummy2 = CallPyFun(pyFunID)
    P = GetPyFunRetVal(0, pyFunID)
    Q = GetPyFunRetVal(1, pyFunID)
"""


class ElectronicLoadWECC:
    """Electronic load component of the WECC composite load model (CMPLDW).

    Power drops linearly between Vd1 and Vd2. Once the voltage has dipped,
    only a fraction frcel of the lost load recovers. Vmin, the lowest
    voltage seen so far, is remembered between calls.

    PowerFactory may call a model more than once per time step. That is
    harmless here: calling calc_pq again with the same V returns the same
    result, because Vmin only ever moves down.
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


eload = ElectronicLoadWECC(Vd1=0.866, Vd2=0.7, frcel=0.7, Pel0=20, Qel0=10)

CALLABLE_REGISTRY = [
    eload.calc_pq,  # pyFunID = 0
]
