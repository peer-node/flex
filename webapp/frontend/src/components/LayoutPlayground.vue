<template>
  <v-container fluid>
    <v-layout column>
      <v-flex>
        <div class="resultContainer">
          <v-layout>
            <div class="item elevation-5"></div>
            <div class="item elevation-5"></div>
            <div class="item elevation-5"></div>
          </v-layout>
        </div>
      </v-flex>
    </v-layout>
  </v-container>
</template>

<script>
  export default {
    name: 'LayoutPlayground.vue'
  }
</script>

<style scoped>
  .resultContainer {
    height: 350px
  }
  .item {
    min-height: 500px;
    min-width: 800px;
    margin: 10px;
  }
</style>
