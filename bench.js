const { run } = require('@run')
const { ANSI } = require('@binary')
const stats = require('stats.js')
const fs = require('fs')

const { writeFile } = fs

async function launch () {
  if (just.opts.flush) {
    writeFile('/proc/sys/vm/drop_caches', ArrayBuffer.fromString('3'))
    await run('sync').waitfor()
  }
  const program = await run('./smol').waitfor()
  if (program.status !== 0) throw new just.SystemError(`exec status ${status}`)
  return Number(program.out)
}

const times = []
const { eraseLine, column } = ANSI.control
const runs = parseInt(just.args[2] || '100', 10)

for (let i = 0; i < runs; i++) {
  const t = await launch()
  just.print(`${column(0)}${eraseLine()}${(i + 1).toString().padEnd(10, ' ')} : ${t}`, false)
  times.push(t)
}

just.print('')
just.print(`mean      ${parseInt(stats.mean(times), 10)} nsec`)
just.print(`median    ${parseInt(stats.median(times), 10)} nsec`)
just.print(`stdDev    ${parseInt(stats.standardDeviation(times), 10)} nsec`)
just.print(`max       ${stats.max(times)} nsec`)
just.print(`min       ${stats.min(times)} nsec`)
just.print(`range     ${stats.range(times)} nsec`)
just.print(`midrange  ${stats.midrange(times)} nsec`)
just.print(`absDev    ${parseInt(stats.meanAbsoluteDeviation(times), 10)} nsec`)
