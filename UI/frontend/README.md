# Frontend set-up

To run the vue.js frontend server you'll need `npm`, 
a prerequisite for which is `nvm`. 

### nvm 
Install [nvm](https://github.com/creationix/nvm#install-script) 
by running the following command: 
```
curl -o- https://raw.githubusercontent.com/creationix/nvm/v0.33.11/install.sh | bash
```
Check installation by issuing `command -v nvm` and confirming that it yields `nvm`.


### node and npm

Install `node` and `npm` with the following 
commands:
```
nvm install node
```
Check installation by executing `which node`
and `which npm` which should yield something like 
`~/.nvm/versions/node/v10.8.0/bin/node` and 
`~/.nvm/versions/node/v10.8.0/bin/npm` respectively.

### Install node modules 

Issue 

```
npm install
```
This will use the 
`package.json` and `package-lock.json` files
to create a directory called `node_modules`
that contains an exact replica of the 
dependencies used by the developers. 

# Spinning up the frontend server
This is as simple as: 
```
npm run serve
```
This runs the [vue-cli-service binary](https://cli.vuejs.org/guide/cli-service.html#cli-service) 
that was installed in the 
`node_modules` directory. 

Navigate to the indicated URL.


# Development

If you intend to contribute to the 
development of the user interface, 
you will need the [vue Command Line 
Interface](https://cli.vuejs.org/guide/installation.html), 
which you can install with `npm`:  
```
npm install -g @vue/cli
```
Check your installation by issuing `which vue` and
`vue --version`, 
which should yield something like 
`~/.nvm/versions/node/v10.8.0/bin/vue`
and `3.0.0` respectively.

The frontend was created from within the `UI` 
directory using 
```
vue create frontend
cd frontend
vue add vuetify
```

To connect the front-end Vue app with 
the back-end Flask app, 
 AJAX requests are sent using 
the axios library, which was installed via: 

```
npm install axios
```


To serve the Vue app and 
hot-load any changes to the source 
code, I used `npm run serve`.




