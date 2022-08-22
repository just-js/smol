if (global.smol) {
  const { error, print } = smol

  global.onUnhandledRejection = err => {
    error(err.stack)
  }

  function wrapHRTime (runtime) {
    const { hrtime, allochrtime } = runtime
    const buf = new ArrayBuffer(8)
    const u32 = new Uint32Array(buf)
    const u64 = new BigUint64Array(buf)
    delete runtime.allochrtime
    const start = Number(runtime.start)
    allochrtime(buf)
    const fun = () => {
      hrtime()
      return Number((u32[1] * 0x100000000) + u32[0]) - start
    }
    fun.bigint = () => {
      hrtime()
      return u64[0]
    }
    return fun
  }

  const hrtime = wrapHRTime(smol)

  const console = {
    log: (...args) => print(...args)
  }

  const process = {
    hrtime
  }

  global.console = console
  global.process = process
}

console.log(process.hrtime())
