# Wordcount Example

This directory contains a simple wordcount example to illustrate the
functionality of the IceFlow library.
The example consists of four nodes (text2lines, lines2words1, lines2words2,
wordcount) that form a very simple stream processing pipeline.
This example corresponds with the one given in the IceFlow paper published by
Kutscher et al. (2021).

The indidividual example applications are compiled when executing

```sh
cmake .
make
```

in the top-level directory.
Afterward, you can run the individual applications by executing the start
scripts from within their respective source directories.

For the setup to work locally, you will need to configure your NFD to use the
multicast strategy.
From the command line, this can be done like so:

```sh
    nfdc strategy set /wordcount /localhost/nfd/strategy/multicast
```

## References

- Dirk Kutscher, Laura Al Wardani, and T M Rayhan Gias. 2021. Vision:
  information-centric dataflow: re-imagining reactive distributed computing.
  In Proceedings of the 8th ACM Conference on Information-Centric Networking
  (ICN '21). Association for Computing Machinery, New York, NY, USA, 52–58.
  https://doi.org/10.1145/3460417.3482975
