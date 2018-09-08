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


# Spinning up the frontend server
This is as simple as: 
```
npm run serve
```
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

The frontend was created from within the `webapp` 
directory using 
```
vue create frontend
```
accepting the default options that the vue CLI 
presented. I then 
 did `cd frontend` followed by
```
vue add vuetify
```
electing to use a 'pre-made template'. 

To connect the front-end Vue app with 
the back-end Flask app, 
I sent AJAX requests using 
the axios library: 

```
npm install axios
```


To serve the Vue app and 
hot-reload any changes to the source 
code, I used `npm run serve`.




