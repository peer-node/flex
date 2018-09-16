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
              :label="switch_state ? `On` : `Off`"
              v-model="switch_state"
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
          <v-btn icon @click="$emit('get-data')">
            <v-icon>refresh</v-icon>
          </v-btn>
        </v-list-tile-action>
      </v-list-tile>
    </v-list>
  </div>
</template>

<script>
  export default {
    name: 'Mining',
    props: ['data'],
    data () {
      return {
        // v-switch requires a variable (switch_state) to maintain its state
        switch_state: this.data.mining
      }
    },
    methods: {
      toggle_mining () {
        const action = this.switch_state ? 'start_mining' : 'stop_mining'
        this.$emit('toggle-mining', action)
      }
    },
  }
</script>

<style scoped>

</style>
