# TangleSim
Omnet++ simulation of a directed acyclic graph

**Note the bulk of the implementation was written in 2018 when I had zero commercial software engineering experience so should be viewed as such.**

This a simulation model that can be used to simulate a network of individual nodes, issuing transactions to construct (via various tip selection methods) a Tangle.

The Tangle is a Directed Acyclic Graph (DAG) which is not a novel structure. However, using a DAG where nodes are individual transactions is a novel use, and introduced to us by the IOTA foundation, see: https://www.iota.org/research/academic-papers .

I produced this model as part of my masters thesis while at Cardiff University, and is part of - an as of yet unpublished - paper to appear at marble2019, see: https://www.marble2019.org/ .

The omnet++ and pure c++ classes are separate yet fairly intertwined. The omnet++ modules should really privately inherit from the pure c++ classes, to avoid some of the hacky reference passing around. 

It is possible to use the pure c++ classes with another event simulator/framework, but as time has gone on, in this version they are more intertwined than ever.

Feel free to drop me a message if you want to contribute/branch/pull etc, or more likely if you want to know how it works - happy to help either way.
