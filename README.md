This project provides a library of different barrier and phaser algorithms to
facilitate synchronization of concurrent programs.

The algorithms are typical textbook implementations or current research
results. They are targeted for multi- and many-core CPUs.

The phaser implementations are meant to facilitate fine-grained fork/join
parallelism and provide advanced control to avoid over-synchronization.

This repository includes the code for the following paper:

  Insertion Tree Phasers: Efficient and Scalable Barrier Synchronization for
  Fine-grained Parallelism, Stefan Marr, Stijn Verhaegen, Bruno De Fraine, Theo
  D’Hondt, Wolfgang De Meuter, Proceedings of the 12th IEEE International
  Conference on High Performance Computing and Communications (HPCC), IEEE CS,
  September (2010)

  http://stefan-marr.de/2010/07/insertion-tree-phasers-efficient-and-scalable-barrier-synchronization-for-fine-grained-parallelism/
