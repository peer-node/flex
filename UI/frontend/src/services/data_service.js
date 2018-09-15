import axios from 'axios'

// https://github.com/axios/axios
const apiClient = axios.create({
  baseURL: 'http://localhost:5000',
  withCredentials: false,
  headers: {
    Accept: 'application/json',
    'Content-Type': 'application/json'
  }
})

const version = 'v1.0'
const api = '/api/' + version + '/'

export default {
  get_data() {
    return apiClient.get(api)
  },
  toggle_mining(action) {
    return apiClient.get(api + action)
  }

}
