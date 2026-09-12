const path = require('path')

module.exports = {
  cli: {
    trimDotnine: true,
    buildNameFormat: 'ORIGINAL',
    optimizeDescMeta: true
  },
  webpack: {
    resolve: {
      alias: {
        '@': path.resolve(__dirname, 'src')
      }
    }
  }
}
