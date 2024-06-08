#!/usr/bin/env python3
# © 2014 Nicolai Waniek <rochus at rochus dot net>, see LICENSE file for details

"""Utility functions for plotting"""

from numpy import arange
import matplotlib.pyplot as plt

def rasterplot(spikes, dt=0.001, ax=None, color='#9a9a9a'):
    """Raster plot some spiketrains. Row data are neurons, columns are
    the individual timesteps. the spikes need to be stored as a dense
    matrix, containing 1s for spike times and 0s otherwise"""
    if ax is None:
        ax = plt.gca()

    # determine number of neurons, and times
    N = spikes.shape[0]
    t = spikes.shape[1]
    tspace = arange(0, t*dt, dt)

    for n in range(N):
        s = spikes[n, :] * tspace
        s = s[s > 0]
        ax.vlines(s, n+0.5, n+1.5, color=color)

    # plt.vlines(spikes[i, :], i+0.5, i+1.5, color=color)
    ax.set_ylim(.5, N + .5)
    ax.set_xlim(-dt, t*dt+dt)
    ax.set_xlabel("Time [ms]")
    ax.set_ylabel('Neuron Index')

    return ax
