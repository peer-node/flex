<template>
  <!-- v-if="data" ensures that data is populated with results of api call
  before any component that uses it is created -->
  <!--https://stackoverflow.com/questions/45059832/vue-2-0-passing-asynchronous-data-to-child-component-->
  <v-container grid-list-xl
               v-if="data">
    <v-layout row>
      <!--span one third of screen width on small devices and above:-->
      <v-flex sm4>
        <Mining :data="data"
                @get-data="get_data"
                @toggle-mining="toggle_mining"></Mining>
      </v-flex>
      <!--the remainder of this template is a collection of placeholders-->
      <v-flex d-flex xs12 sm6 md3>
        <v-layout row wrap>
          <v-flex>
            <v-card color="indigo" dark>
              <v-card-text>{{ lorem.slice(0, 700) }}</v-card-text>
              <v-card-text>{{ data }}</v-card-text>
            </v-card>
          </v-flex>
          <v-flex d-flex>
            <v-layout row wrap>
              <v-flex
                  v-for="n in 2"
                  :key="n"
                  d-flex
                  xs12>
                <v-card
                    color="red lighten-2"
                    dark>
                  <v-card-text>{{ lorem.slice(0, 40) }}</v-card-text>
                </v-card>
              </v-flex>
            </v-layout>
          </v-flex>
        </v-layout>
      </v-flex>
      <v-flex d-flex xs12 sm6 md2 child-flex>
        <v-card color="green lighten-2" dark>
          <v-card-text>{{ lorem.slice(0, 90) }}</v-card-text>
        </v-card>
      </v-flex>
      <v-flex d-flex xs12 sm6 md3>
        <v-card color="blue lighten-2" dark>
          <v-card-text>{{ lorem.slice(0, 100) }}</v-card-text>
        </v-card>
      </v-flex>
    </v-layout>
  </v-container>
</template>

<script>
  import Mining from '@/components/Mining'
  import data_service from '@/services/data_service'

  export default {
    name: 'Panels',
    data () {
      return {
        data: false,
        lorem: `Lorem ipsum dolor sit amet, mel at clita quando. Te sit oratio vituperatoribus, nam ad ipsum posidonium mediocritatem, explicari dissentiunt cu mea. Repudiare disputationi vim in, mollis iriure nec cu, alienum argumentum ius ad. Pri eu justo aeque torquatos.`,
      }
    },
    components: {
      Mining,
    },
    methods: {
      get_data () {
        data_service.get_data().then(response => {
          this.data = response.data
        }).catch(error => {
          alert(error)
        })
      },
      toggle_mining (action) {
        data_service.toggle_mining(action).then(response => {
          this.data = response.data
        }).catch(error => {
          alert(error)
        })
      }
    },
    created () {
      this.get_data()
    },
  }
</script>

<style scoped>

</style>
