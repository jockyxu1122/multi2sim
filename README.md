# NN_cache_replacement

### Preparation:

  - Replace `System.Events.cc` under `multi2sim/src/memory`.
  - Replace `Context.cc` under `multi2sim/src/arch/x86/emulator`.
  - Put `mem-config` under `multi2sim/samples/x86/example-x`.
  - Modify `x86-config.ini` with desired cores and threads.

### To run:
```sh
$ cd multi2sim
$ make
$ rm -f trainingData.txt
$ cd <one of the bechmark folders>
$ m2s --x86-config x86-config.ini --x86-sim detailed --ctx-config ctx-config.ini
```

## Important!
Remember to do:
```sh
rm -f trainingData.txt
```
before running, since the code merely appends lines at EOF of _trianingData.txt_.

### trainingData.txt
  - Each program is separated by `\nNext program:\n\n`.
  - Only L2 shared cache related data is logged.
  - Format:
  
    __\<cache name> \<cache hit(1)/miss(0)> \<cache id> \<cache address> \<cache tag> \<cache set(index)> \<cache way(offset)> \<cache state>__
  - Cache state map:
    {

      { "N", BlockNonCoherent },

      { "M", BlockModified },

      { "O", BlockOwned },

      { "E", BlockExclusive },

      { "S", BlockShared },

      { "I", BlockInvalid }

    }