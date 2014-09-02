var connect = require('connect')
var serveStatic = require('serve-static')
var app = connect();
var port = process.env.NODE_PROT || 8000;
app.use(serveStatic('app/', {'index': ['index.html']}))
app.use(function(req, res){
  res.end('Hello World!\n');
});
app.listen(port);
