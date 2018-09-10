<template>
  <v-app dark>
    <v-toolbar>
      <v-toolbar-title>teleport_logo</v-toolbar-title>
    </v-toolbar>
    <v-content>
      <Panels></Panels>
    </v-content>
  </v-app>
</template>

<script>
  import Panels from '@/views/Panels'

  export default {
    name: 'App',
    data () {
      return {
      }
    },
    components: {
      Panels
    }
  }
</script>
