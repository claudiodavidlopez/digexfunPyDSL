import zmq
from _script import ElectronicLoadWECC


eload = ElectronicLoadWECC(0.866, 0.7, 0.7,
                           20, 10)

context = zmq.Context()
socket = context.socket(zmq.REP)
socket.bind('tcp://*:7000')

while True:
    sim_ins = socket.recv_json()
    V = sim_ins[0]
    P, Q = eload.calc_pq(V)
    sim_outs = [P, Q]
    socket.send_json(sim_outs)
