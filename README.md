# trinity
WebGPU implementation of Dawn, using A-type

this is done in C and designed for interop with silver.  ux things we'll implement in C and use with silver.  reading schema is only trivial when we have a pre-processor macro
system to emit them.

# build meta, so classes emit some data files for silver lol
# we could accurately reconstruct an ABI bridge
# make this the most compact structure.  this is definitely something to do now.
# its about making A-type available to silver and thats kind of cool
# import would work with these schema files, but what extension should it be?

# its simple member serialization, member-name, member-type (str or index),