set(BASE_URL "http://orthanc.osimis.io/ThirdPartyDownloads")

DownloadPackage(
  "da0189f7c33bf9f652ea65401e0a3dc9"
  "${BASE_URL}/dicom-web/bootstrap-4.3.1.zip"
  "${CMAKE_CURRENT_BINARY_DIR}/bootstrap-4.3.1")

DownloadPackage(
  "8242afdc5bd44105d9dc9e6535315484"
  "${BASE_URL}/dicom-web/vuejs-2.6.10.tar.gz"
  "${CMAKE_CURRENT_BINARY_DIR}/vue-2.6.10")

DownloadPackage(
  "3e2b4e1522661f7fcf8ad49cb933296c"
  "${BASE_URL}/dicom-web/axios-0.19.0.tar.gz"
  "${CMAKE_CURRENT_BINARY_DIR}/axios-0.19.0")

DownloadFile(
  "220afd743d9e9643852e31a135a9f3ae"
  "${BASE_URL}/jquery-3.4.1.min.js")


set(JAVASCRIPT_LIBS_DIR  ${CMAKE_CURRENT_BINARY_DIR}/javascript-libs)
file(MAKE_DIRECTORY ${JAVASCRIPT_LIBS_DIR})

file(COPY
  ${CMAKE_CURRENT_BINARY_DIR}/axios-0.19.0/dist/axios.min.js
  ${CMAKE_CURRENT_BINARY_DIR}/axios-0.19.0/dist/axios.min.map
  ${CMAKE_CURRENT_BINARY_DIR}/bootstrap-4.3.1/dist/js/bootstrap.min.js
  ${CMAKE_CURRENT_BINARY_DIR}/bootstrap-4.3.1/dist/js/bootstrap.min.js.map
  ${CMAKE_CURRENT_BINARY_DIR}/vue-2.6.10/dist/vue.min.js
  ${CMAKE_SOURCE_DIR}/ThirdPartyDownloads/jquery-3.4.1.min.js
  DESTINATION
  ${JAVASCRIPT_LIBS_DIR}/js
  )

file(COPY
  ${CMAKE_CURRENT_BINARY_DIR}/bootstrap-4.3.1/dist/css/bootstrap.min.css
  ${CMAKE_CURRENT_BINARY_DIR}/bootstrap-4.3.1/dist/css/bootstrap.min.css.map
  DESTINATION
  ${JAVASCRIPT_LIBS_DIR}/css
  )
