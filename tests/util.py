import logging

import numpy as np

LOGGER = logging.getLogger(__name__)


def print_mat(mat: list[list], rjust: bool = True):
    if not mat:
        LOGGER.warning("Empty matrix")
        return

    max_lengths = [max(len(str(row[i])) for row in mat) for i in range(len(mat[0]))]

    for row in mat:
        ret = ""
        for i, cell in enumerate(row):
            if rjust:
                ret += str(cell).rjust(max_lengths[i]) + "  "
            else:
                ret += str(cell).ljust(max_lengths[i]) + "  "
        LOGGER.info(ret)

def to_dense(X, n=None, d=None):
    n = len(X) if n is None else n
    d = max(max(d.keys()) for d in X) + 1 if d is None else d
    X_dense = np.zeros((n, d))
    for i, x_sparse in enumerate(X):
        for k, v in x_sparse.items():
            X_dense[i][k] = v

    return X_dense