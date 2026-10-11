"""Link-time optimization for project sources; libraries/ESP-IDF keep their own flags."""

Import("env")
env.Append(LINKFLAGS=["-flto"])
