<template>
  <div>
    <v-toolbar>
      <v-toolbar-title>Mining</v-toolbar-title>
    </v-toolbar>
    <v-list two-line>
      <v-list-tile>
        <v-list-tile-action>
          <!--https://github.com/vuetifyjs/vuetify/issues/3554-->
          <v-switch
              :label="mining ? `on` : `off`"
              v-model="mining"
              @change="toggle_mining"
          ></v-switch>
        </v-list-tile-action>
      </v-list-tile>
      <v-list-tile>
        <v-list-tile-content>
          <v-list-tile-title>Credits</v-list-tile-title>
          <v-list-tile-sub-title>{{ data.info.balance }}</v-list-tile-sub-title>
        </v-list-tile-content>
        <v-list-tile-action>
          <v-btn icon @click="fetch_data">
            <v-icon>refresh</v-icon>
          </v-btn>
        </v-list-tile-action>
      </v-list-tile>
    </v-list>
  </div>
</template>

<script>
  import axios from 'axios'

  export default {
    name: 'Mining',
    data: () => ({
      mining: false
    }),
    methods: {
      // TODO: put this in Panels.vue
      fetch_data () {
        const path = `http://localhost:5000/api/v1.0`
        axios
        .get(path)
        .then(response => {
          this.data = response.data
        })
        .catch(error => {
          alert(error)
        })
      },
      toggle_mining () {
        const path_suffix = this.mining ? 'start_mining' : 'stop_mining'
        const path = `http://localhost:5000/api/v1.0/${path_suffix}`
        axios
        .get(path)
        .then(response => {
          // TODO: "emit" updated data to other components
          this.data = response.data
        })
        .catch(error => {
          alert(error)
        })
      }
    },
    props: {
      data: Object
    }
  }
</script>

<style scoped>

</style>
