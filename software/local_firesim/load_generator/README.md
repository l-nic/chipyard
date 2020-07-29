Setup instructions:

This is modeled after local firesim. Follow the setup instructions in README.md in the local_firesim directory in order to use the load generator.

Notes:

This load generator is based on the software switch in chipyard/sims/firesim/target-design/switch, but uses unique files. Unlike local firesim, which contains symbolic links to the actual firesim switch (and should therefore be nearly identical to it aside from switchconfig.h), this load generator contains copies of the original files that will gradually diverge over time. This means that, for now, the load generator will work in local firesim only, and not in the full AWS firesim.
