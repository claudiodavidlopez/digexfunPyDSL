import random
import pandas as pd
import numpy as np
import sklearn.discriminant_analysis as skda


LDA = skda.LinearDiscriminantAnalysis


def fprintf(s, file_name):
    with open(file_name, 'a') as f:
        f.write(s+'\n')


def load_dataframe(file_name):
    return pd.read_csv(file_name, sep=',', decimal='.')


def label_dataframe(data, start_sc, end_sc, sc_label=1, pre_sc_label=-2,
                    post_sc_label=-1):
     data.loc[(data['t']<start_sc) , 'label'] = pre_sc_label
     data.loc[(data['t']>=start_sc) & (data['t']<=end_sc), 'label'] = sc_label
     data.loc[(data['t']>end_sc), 'label'] = post_sc_label
     return data


def load_training_data():
    dataset = pd.DataFrame()
    for bus_number in range(1, 7):
        for R in range(0, 50, 10):
            file_name = 'training_data/B%d_R%d.csv'%(bus_number, R)
            df = load_dataframe(file_name)
            df = label_dataframe(df, 0.1, 0.2, sc_label=bus_number)
            dataset = dataset.append(df, ignore_index=True)

    labels = dataset['label'].values
    dataset = dataset.drop(['t', 'label'], axis=1).values
    return dataset, labels


class ElectronicLoadWECC:
    def __init__(self, Vd1, Vd2, frcel,
                 Pel0, Qel0, Vmin0=1.0):
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
                Fvl = (V - self.Vd2)/(self.Vd1
                       - self.Vd2)
            else:
                Fvl = (self.Vmin - self.Vd2
                       + self.frcel*(V
                       - self.Vmin))/(self.Vd1
                       - self.Vd2)
        else:
            if self.Vmin >= self.Vd1:
                Fvl = 1.0
            else:
                Fvl = (self.Vmin - self.Vd2
                       + self.frcel*(self.Vd1
                       - self.Vmin))/(self.Vd1
                       - self.Vd2)

        return Fvl*self.Pel0, Fvl*self.Qel0


eload = ElectronicLoadWECC(0.866, 0.7, 0.7,
                           20, 10)


def random_load(Pnom, Qnom, max_dev):
    p_dev = random.uniform(-max_dev, max_dev)
    q_dev = random.uniform(-max_dev, max_dev)
    P = (1 + p_dev)*Pnom
    Q = (1 + q_dev)*Qnom
    return P, Q


class FaultDetector:
    def __init__(self, n_buses):
        self.n_buses = n_buses
        data, labels = load_training_data()
        self.lda = LDA()
        self.lda = self.lda.fit(data, labels)

    def detect(self, *args):
        fault_flags = [0.0]*self.n_buses
        label = int(self.lda.predict([args]))
        if label > 0:
            fault_flags[label-1] = 1.0

        return tuple(fault_flags)


fdetector = FaultDetector(n_buses=6)


CALLABLE_REGISTRY = [
    eload.calc_pq,    # pyFunID = 0
    random_load,      # pyFunID = 1
    random_load,      # pyFunID = 2
    random_load,      # pyFunID = 3
    fdetector.detect, # pyFunID = 4
]
