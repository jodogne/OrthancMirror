/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2021 Osimis S.A., Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


new Vue({
  el: '#app',
  data: {
    dicomNodes: {},
    peers: [],
    canTestPeers: false,
    dicomWebServers: []
  },
  methods: {
    toggle: function (todo) {
      todo.done = !todo.done
    },

    testDicomModalities: function () {
      console.log('testing DICOM modalities');
      axios
        .get('../../modalities?expand')
        .then(response => {
          this.dicomNodes = response.data;
          for (let alias of Object.keys(this.dicomNodes)) {
            this.dicomNodes[alias]['alias'] = alias;
            this.dicomNodes[alias]['status'] = 'testing';
            axios
              .post('../../modalities/' + alias + '/echo', {})
              .then(response => {
                this.dicomNodes[alias]['status'] = 'ok';
                this.$forceUpdate();
              })
              .catch(response => {
                this.dicomNodes[alias]['status'] = 'ko';
                this.$forceUpdate();
              })
                }
        })
    },

    testOrthancPeers: function () {
      console.log('testing Orthanc peers');
      axios
        .get('../../peers?expand')
        .then(response => {
          this.peers = response.data;
          for (let alias of Object.keys(this.peers)) {
            this.peers[alias]['alias'] = alias;

            if (this.canTestPeers) {
              this.peers[alias]['status'] = 'testing';
              axios
                .get('../../peers/' + alias + '/system') // introduced in ApiVersion 5 only !
                .then(response => {
                  this.peers[alias]['status'] = 'ok';
                  this.$forceUpdate();
                })
                .catch(response => {
                  this.peers[alias]['status'] = 'ko';
                  this.$forceUpdate();
                })
                  }
            else {
              this.peers[alias]['status'] = 'unknown';
              this.$forceUpdate();
            }
          }
        })
    },

    testDicomWebServers: function () {
      console.log('testing Dicom-web servers');
      axios
        .get('../../dicom-web/servers?expand')
        .then(response => {
          this.dicomWebServers = response.data;
          for (let alias of Object.keys(this.dicomWebServers)) {
            this.dicomWebServers[alias]['alias'] = alias;
            this.dicomWebServers[alias]['status'] = 'testing';

            // perform a dummy qido-rs to test the connectivity
            axios
              .post('../../dicom-web/servers/' + alias + '/qido', {
                'Uri' : '/studies',
                'Arguments' : {
                  '00100010' : 'CONNECTIVITY^CHECKS'
                }
              })
              .then(response => {
                this.dicomWebServers[alias]['status'] = 'ok';
                this.$forceUpdate();
              })
              .catch(response => {
                this.dicomWebServers[alias]['status'] = 'ko';
                this.$forceUpdate();
              })
                }
        })
    },

  },
  computed: {
  },
  mounted() {
    axios
      .get('../../system')
      .then(response => {
        this.canTestPeers = response.data.ApiVersion >= 5;
        this.testDicomModalities();
        if (this.canTestPeers) {
          this.testOrthancPeers();
        }
        this.testDicomWebServers();
      })
  }
})
