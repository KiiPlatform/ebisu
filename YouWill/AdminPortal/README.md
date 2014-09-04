#YouWill Admin Portal
##Guide for Deployment
###Things you need to install before deployment
####1. install [NodeJs](http://nodejs.org/)
####2. install bower locally
`npm install -g bower`
####3. install forever locally
`npm install -g forever`
####4. install dependencies
clone the project by `git clone <project_url>`

`cd` to the directory of this project folder and run:
* `npm install` and 
* `bower install --allow-root`

####5. add KiiSDK
manually put `KiiSDK.js` into `./app/bower_components/KiiSDK/`
### Deploy the project
`forever start app.js`, to change the port number: edit `./app.js`
### Stop the project
`forever stop app.js`
### More about `forever`
see this [link](https://github.com/nodejitsu/forever)
##Guide for update
###Pull the latest update
`git pull`
###install dependencies if needed
* `npm install` and 
* `bower install --allow-root`

###restart server
`forever restart app.js`
