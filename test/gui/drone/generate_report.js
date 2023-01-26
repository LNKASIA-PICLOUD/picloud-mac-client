// clang-format off
const fs = require('fs')
const path = require('path')
const process = require('process')

const config = {
  reportFile: 'results-v1.js',
  reportDir: '',
  server: '',
}

function getResultFile() {
  const args = Object.values(process.argv)
  // first element is the node executable
  // second element is the script path
  // strip the first two elements
  args.shift()
  args.shift()
  const resultDir = path.resolve(args[0])
  config.server = args[1] ?? ''

  if (!resultDir) {
    throw new Error('[Error] Report directory path not provided')
  }
  if (!fs.existsSync(resultDir)) {
    throw new Error('[Error] Cannot locate path: ' + resultDir)
  }

  if (fs.lstatSync(resultDir).isFile()) {
    if (path.extname(resultDir) !== '.js') {
      throw new Error(
        '[Error] Invalid report file: ' + resultDir + '\n(MUST be a .js file)'
      )
    }
    return resultDir
  }

  config.reportDir = resultDir
  // report file is located in the 'data' subdirectory
  let resultFile = path.join(resultDir, 'data', config.reportFile)

  if (!fs.existsSync(resultFile)) {
    console.info('Report file not found: ' + resultFile)
    // try to find report file in the root of the report directory
    resultFile = path.join(resultDir, config.reportFile)
    console.info('Retrying with the path: ' + resultFile)
    if (!fs.existsSync(resultFile)) {
      throw new Error('Report file not found: ' + resultFile)
    }
  }
  return resultFile
}

function parse(file) {
  let content = fs.readFileSync(file, 'utf8')
  content += '\n;(()=> data)();'
  return eval(content)
}

function generateResult(scenario) {
  let result = { scenario: scenario.name }
  if (!scenario.isSkipped) {
    result.status = 'PASS'
    for (const step of scenario.tests) {
      if (step.tests && step.tests.length) {
        for (const error of step.tests) {
          if (error.result === 'ERROR' || error.result === 'FAIL') {
            result.status = 'FAIL'
          }
        }
      }
    }
  } else {
    result.status = 'SKIP'
  }
  return result
}

function generateReport(result) {
  const report = {}
  for (const suite of result.tests) {
    for (const tstCase of suite.tests) {
      const test_case_report = []
      for (const feature of tstCase.tests) {
        for (const scenario of feature.tests) {
          if (scenario.type == 'scenariooutline') {
            for (const example of scenario.tests) {
              const result = generateResult(example)
              result.scenario = scenario.name
              result.example = example.name
              test_case_report.push(result)
            }
          } else {
            const result = generateResult(scenario)
            test_case_report.push(result)
          }
        }
      }
      report[tstCase.name] = test_case_report
    }
  }
  return report
}

// function textReport(report) {
//   log = ['+' + '-'.repeat(80) + '+']
//   for (const tstCase in report) {
//     log.push(`| ${tstCase}` + ' '.repeat(66) + '|')
//     log.push('+' + '-'.repeat(80) + '+')
//     report[tstCase].forEach((test, idx) => {
//       log.push(`| ${idx + 1}. | ${test.scenario} | ${test.status} |`)
//       log.push('|' + '_'.repeat(80) + '|')
//     })
//   }
//   console.log(log.join('\n'))
//   return log.join('\n')
// }

function htmlReport(report) {
  const html = [
    `<style>
body{font-family:sans-serif; padding:8px; color:#384c6b; width:85vw; margin:auto;}
table,td{border:solid 1px #384c6b; border-collapse:collapse; width:100%;}
th,td{padding:8px; text-align:left;}
th{background:#384c6b; color:white;}
.pass{background:#1ead1e; color:white;}
.fail{background:red; color:white;}
.skip{background:gray; color:white;}
.sn{width:auto;}
</style>`,
    '<body>',
    `<h1>Squish GUI Test Report (server: ${config.server})</h1>`,
    '<table>',
  ]
  for (const tstCase in report) {
    html.push(`<tr><th colspan="3">${tstCase}</th></tr>`)
    report[tstCase].forEach((test, idx) => {
      let scenarioTitle = test.scenario
      scenarioTitle += test.example
        ? `<br/><i><small>(${test.example})</small></i>`
        : ''
      html.push('<tr>')
      html.push(`<td class="sn">${idx + 1}.</td>`)
      html.push(`<td>${scenarioTitle}</td>`)
      html.push(
        `<td class="${test.status.toLowerCase()}">${test.status.toUpperCase()}</td>`
      )
      html.push('</tr>')
    })
  }
  html.push('</table></body>')
  const htmlContent = html.join('\n')
  fs.writeFileSync(
    path.join(config.reportDir, 'minimal-report.html'),
    htmlContent
  )
}

function parseResult() {
  const resultFile = getResultFile()

  if (!fs.existsSync(resultFile)) {
    throw new Error('Report file not found: ' + resultFile)
  }
  if (!fs.lstatSync(resultFile).isFile()) {
    throw new Error('Provided path is not a file: ' + resultFile)
  }
  if (path.extname(resultFile) !== '.js') {
    // TODO: parse results from the file content
    throw new Error('Provided path is not a JS file: ' + resultFile)
  }

  const results = parse(resultFile)
  for (const result of results) {
    const report = generateReport(result)
    htmlReport(report)
    // generate reports for the first test suite
    // TODO: generate reports for multiple test suites
    break
  }
}

parseResult()
