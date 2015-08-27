// http://stackoverflow.com/questions/1663741/is-there-a-good-jquery-drag-and-drop-file-upload-plugin


// Forbid the access to IE
if ($.browser.msie)
{
  alert("Please use Mozilla Firefox or Google Chrome. Microsoft Internet Explorer is not supported.");
}

// http://jquerymobile.com/demos/1.1.0/docs/api/globalconfig.html
//$.mobile.ajaxEnabled = false;
//$.mobile.page.prototype.options.addBackBtn = true;
//$.mobile.defaultPageTransition = 'slide';


var currentPage = '';
var currentUuid = '';


// http://stackoverflow.com/a/4673436
String.prototype.format = function() {
  var args = arguments;
  return this.replace(/{(\d+)}/g, function(match, number) { 
    /*return typeof args[number] != 'undefined'
      ? args[number]
      : match;*/

    return args[number];
  });
};


function Refresh()
{
  if (currentPage == 'patient')
    RefreshPatient();
  else if (currentPage == 'study')
    RefreshStudy();
  else if (currentPage == 'series')
    RefreshSeries();
  else if (currentPage == 'instance')
    RefreshInstance();
}


$(document).ready(function() {
  var $tree = $('#dicom-tree');
  $tree.tree({
    autoEscape: false
  });

  $('#dicom-tree').bind(
    'tree.click',
    function(event) {
      if (event.node.is_open)
        $tree.tree('closeNode', event.node, true);
      else
        $tree.tree('openNode', event.node, true);
    }
  );
  
  currentPage = $.mobile.pageData.active;
  currentUuid = $.mobile.pageData.uuid;
  if (!(typeof currentPage === 'undefined') &&
      !(typeof currentUuid === 'undefined') &&
      currentPage.length > 0 && 
      currentUuid.length > 0)
  {
    Refresh();
  }
});


function SplitLongUid(s)
{
  return '<span>' + s.substr(0, s.length / 2) + '</span> <span>' + s.substr(s.length / 2, s.length - s.length / 2) + '</span>';
}


function ParseDicomDate(s)
{
  y = parseInt(s.substr(0, 4), 10);
  m = parseInt(s.substr(4, 2), 10) - 1;
  d = parseInt(s.substr(6, 2), 10);

  if (y == null || m == null || d == null ||
      !isFinite(y) || !isFinite(m) || !isFinite(d))
  {
    return null;
  }

  if (y < 1900 || y > 2100 ||
      m < 0 || m >= 12 ||
      d <= 0 || d >= 32)
  {
    return null;
  }

  return new Date(y, m, d);
}


function FormatDicomDate(s)
{
  if (s == undefined)
    return "No date";

  var d = ParseDicomDate(s);
  if (d == null)
    return '?';
  else
    return d.toString('dddd, MMMM d, yyyy');
}


function Sort(arr, fieldExtractor, isInteger, reverse)
{
  var defaultValue;
  if (isInteger)
    defaultValue = 0;
  else
    defaultValue = '';

  arr.sort(function(a, b) {
    var ta = fieldExtractor(a);
    var tb = fieldExtractor(b);
    var order;

    if (ta == undefined)
      ta = defaultValue;

    if (tb == undefined)
      tb = defaultValue;

    if (isInteger)
    {
      ta = parseInt(ta, 10);
      tb = parseInt(tb, 10);
      order = ta - tb;
    }
    else
    {
      if (ta < tb)
        order = -1;
      else if (ta > tb)
        order = 1;
      else
        order = 0;
    }

    if (reverse)
      return -order;
    else
      return order;
  });
}


function SortOnDicomTag(arr, tag, isInteger, reverse)
{
  return Sort(arr, function(a) { 
    return a.MainDicomTags[tag];
  }, isInteger, reverse);
}



function GetResource(uri, callback)
{
  $.ajax({
    url: '..' + uri,
    dataType: 'json',
    async: false,
    cache: false,
    success: function(s) {
      callback(s);
    }
  });
}


function CompleteFormatting(s, link, isReverse)
{
  if (link != null)
  {
    s = 'href="' + link + '">' + s + '</a>';
    
    if (isReverse)
      s = 'data-direction="reverse" '+ s;

    s = '<a ' + s;
  }

  if (isReverse)
    return '<li data-icon="back">' + s + '</li>';
  else
    return '<li>' + s + '</li>';
}


function FormatMainDicomTags(tags, tagsToIgnore)
{
  var s = '';

  for (var i in tags)
  {
    if (tagsToIgnore.indexOf(i) == -1)
    {
      var v = tags[i];

      if (i == "PatientBirthDate" ||
          i == "StudyDate" ||
          i == "SeriesDate")
      {
        v = FormatDicomDate(v);
      }
      else if (i == "DicomStudyInstanceUID" ||
               i == "DicomSeriesInstanceUID")
      {
        v = SplitLongUid(v);
      }
      

      s += ('<p>{0}: <strong>{1}</strong></p>').format(i, v);
    }
  }

  return s;
}


function FormatPatient(patient, link, isReverse)
{
  var s = ('<h3>{0}</h3>{1}' + 
           '<span class="ui-li-count">{2}</span>'
          ).format
  (patient.MainDicomTags.PatientName,
   FormatMainDicomTags(patient.MainDicomTags, [ 
     "PatientName", 
     "OtherPatientIDs" 
   ]),
   patient.Studies.length
  );

  return CompleteFormatting(s, link, isReverse);
}



function FormatStudy(study, link, isReverse)
{
  var s = ('<h3>{0}</h3>{1}' +
           '<span class="ui-li-count">{2}</span>'
           ).format
  (study.MainDicomTags.StudyDescription,
   FormatMainDicomTags(study.MainDicomTags, [
     "StudyDescription", 
     "StudyTime" 
   ]),
   study.Series.length
  );

  return CompleteFormatting(s, link, isReverse);
}



function FormatSeries(series, link, isReverse)
{
  var c;
  if (series.ExpectedNumberOfInstances == null ||
      series.Instances.length == series.ExpectedNumberOfInstances)
  {
    c = series.Instances.length;
  }
  else
  {
    c = series.Instances.length + '/' + series.ExpectedNumberOfInstances;
  }

  var s = ('<h3>{0}</h3>' +
           '<p><em>Status: <strong>{1}</strong></em></p>{2}' +
           '<span class="ui-li-count">{3}</span>').format
  (series.MainDicomTags.SeriesDescription,
   series.Status,
   FormatMainDicomTags(series.MainDicomTags, [
     "SeriesDescription", 
     "SeriesTime", 
     "Manufacturer",
     "ImagesInAcquisition",
     "SeriesDate"
   ]),
   c
  );

  return CompleteFormatting(s, link, isReverse);
}


function FormatInstance(instance, link, isReverse)
{
  var s = ('<h3>Instance {0}</h3>{1}').format
  (instance.IndexInSeries,
   FormatMainDicomTags(instance.MainDicomTags, [
     "AcquisitionNumber", 
     "InstanceNumber", 
     "InstanceCreationDate", 
     "InstanceCreationTime"
   ])
  );

  return CompleteFormatting(s, link, isReverse);
}


$('[data-role="page"]').live('pagebeforeshow', function() {
  $.ajax({
    url: '../system',
    dataType: 'json',
    async: false,
    cache: false,
    success: function(s) {
      if (s.Name != "") {
        $('.orthanc-name').html('<a class="ui-link" href="explorer.html">' + s.Name + '</a> &raquo; ');
      }
    }
  });
});



$('#find-patients').live('pagebeforeshow', function() {
  GetResource('/patients?expand', function(patients) {
      var target = $('#all-patients');
      $('li', target).remove();
    
      SortOnDicomTag(patients, 'PatientName', false, false);

      for (var i = 0; i < patients.length; i++) {
        var p = FormatPatient(patients[i], '#patient?uuid=' + patients[i].ID);
        target.append(p);
      }

      target.listview('refresh');
  });
});



function SetupAnonymizedOrModifiedFrom(buttonSelector, resource, resourceType, field)
{
  if (field in resource)
  {
    $(buttonSelector).closest('li').show();
    $(buttonSelector).click(function(e) {
      window.location.assign('explorer.html#' + resourceType + '?uuid=' + resource[field]);
    });
  }
  else
  {
    $(buttonSelector).closest('li').hide();
  }
}



function RefreshPatient()
{
  if ($.mobile.pageData) {
    GetResource('/patients/' + $.mobile.pageData.uuid, function(patient) {
      GetResource('/patients/' + $.mobile.pageData.uuid + '/studies', function(studies) {
        SortOnDicomTag(studies, 'StudyDate', false, true);

        $('#patient-info li').remove();
        $('#patient-info')
          .append('<li data-role="list-divider">Patient</li>')
          .append(FormatPatient(patient))
          .listview('refresh');

        var target = $('#list-studies');
        $('li', target).remove();
        
        for (var i = 0; i < studies.length; i++) {
          if (i == 0 || studies[i].MainDicomTags.StudyDate != studies[i - 1].MainDicomTags.StudyDate)
          {
            target.append('<li data-role="list-divider">{0}</li>'.format
                          (FormatDicomDate(studies[i].MainDicomTags.StudyDate)));
          }

          target.append(FormatStudy(studies[i], '#study?uuid=' + studies[i].ID));
        }

        SetupAnonymizedOrModifiedFrom('#patient-anonymized-from', patient, 'patient', 'AnonymizedFrom');
        SetupAnonymizedOrModifiedFrom('#patient-modified-from', patient, 'patient', 'ModifiedFrom');

        target.listview('refresh');

        // Check whether this patient is protected
        $.ajax({
          url: '../patients/' + $.mobile.pageData.uuid + '/protected',
          type: 'GET',
          dataType: 'text',
          async: false,
          cache: false,
          success: function (s) {
            var v = (s == '1') ? 'on' : 'off';
            $('#protection').val(v).slider('refresh');
          }
        });

        currentPage = 'patient';
        currentUuid = $.mobile.pageData.uuid;
      });
    });
  }
}


function RefreshStudy()
{
  if ($.mobile.pageData) {
    GetResource('/studies/' + $.mobile.pageData.uuid, function(study) {
      GetResource('/patients/' + study.ParentPatient, function(patient) {
        GetResource('/studies/' + $.mobile.pageData.uuid + '/series', function(series) {
          SortOnDicomTag(series, 'SeriesDate', false, true);

          $('#study .patient-link').attr('href', '#patient?uuid=' + patient.ID);
          $('#study-info li').remove();
          $('#study-info')
            .append('<li data-role="list-divider">Patient</li>')
            .append(FormatPatient(patient, '#patient?uuid=' + patient.ID, true))
            .append('<li data-role="list-divider">Study</li>')
            .append(FormatStudy(study))
            .listview('refresh');

          SetupAnonymizedOrModifiedFrom('#study-anonymized-from', study, 'study', 'AnonymizedFrom');
          SetupAnonymizedOrModifiedFrom('#study-modified-from', study, 'study', 'ModifiedFrom');

          var target = $('#list-series');
          $('li', target).remove();
          for (var i = 0; i < series.length; i++) {
            if (i == 0 || series[i].MainDicomTags.SeriesDate != series[i - 1].MainDicomTags.SeriesDate)
            {
              target.append('<li data-role="list-divider">{0}</li>'.format
                            (FormatDicomDate(series[i].MainDicomTags.SeriesDate)));
            }
            target.append(FormatSeries(series[i], '#series?uuid=' + series[i].ID));
          }
          target.listview('refresh');

          currentPage = 'study';
          currentUuid = $.mobile.pageData.uuid;
        });
      });
    });
  }
}
  

function RefreshSeries() 
{
  if ($.mobile.pageData) {
    GetResource('/series/' + $.mobile.pageData.uuid, function(series) {
      GetResource('/studies/' + series.ParentStudy, function(study) {
        GetResource('/patients/' + study.ParentPatient, function(patient) {
          GetResource('/series/' + $.mobile.pageData.uuid + '/instances', function(instances) {
            Sort(instances, function(x) { return x.IndexInSeries; }, true, false);

            $('#series .patient-link').attr('href', '#patient?uuid=' + patient.ID);
            $('#series .study-link').attr('href', '#study?uuid=' + study.ID);

            $('#series-info li').remove();
            $('#series-info')
              .append('<li data-role="list-divider">Patient</li>')
              .append(FormatPatient(patient, '#patient?uuid=' + patient.ID, true))
              .append('<li data-role="list-divider">Study</li>')
              .append(FormatStudy(study, '#study?uuid=' + study.ID, true))
              .append('<li data-role="list-divider">Series</li>')
              .append(FormatSeries(series))
              .listview('refresh');

            SetupAnonymizedOrModifiedFrom('#series-anonymized-from', series, 'series', 'AnonymizedFrom');
            SetupAnonymizedOrModifiedFrom('#series-modified-from', series, 'series', 'ModifiedFrom');

            var target = $('#list-instances');
            $('li', target).remove();
            for (var i = 0; i < instances.length; i++) {
              target.append(FormatInstance(instances[i], '#instance?uuid=' + instances[i].ID));
            }
            target.listview('refresh');

            currentPage = 'series';
            currentUuid = $.mobile.pageData.uuid;
          });
        });
      });
    });
  }
}



function ConvertForTree(dicom)
{
  var result = [];

  for (var i in dicom) {
    if (dicom[i] != null) {
      var label = i + '<span class="tag-name"> (<i>' + dicom[i]["Name"] + '</i>)</span>: ';

      if (dicom[i]["Type"] == 'String')
      {
        result.push({
          label: label + '<strong>' + dicom[i]["Value"] + '</strong>',
          children: []
        });
      }
      else if (dicom[i]["Type"] == 'TooLong')
      {
        result.push({
          label: label + '<i>Too long</i>',
          children: []
        });
      }
      else if (dicom[i]["Type"] == 'Null')
      {
        result.push({
          label: label + '<i>Null</i>',
          children: []
        });
      }
      else if (dicom[i]["Type"] == 'Sequence')
      {
        var c = [];
        for (var j = 0; j < dicom[i]["Value"].length; j++) {
          c.push({
            label: 'Item ' + j,
            children: ConvertForTree(dicom[i]["Value"][j])
          });
        }

        result.push({
          label: label + '[]',
          children: c
        });
      }
    }
  }

  return result;
}


function RefreshInstance()
{
  if ($.mobile.pageData) {
    GetResource('/instances/' + $.mobile.pageData.uuid, function(instance) {
      GetResource('/series/' + instance.ParentSeries, function(series) {
        GetResource('/studies/' + series.ParentStudy, function(study) {
          GetResource('/patients/' + study.ParentPatient, function(patient) {

            $('#instance .patient-link').attr('href', '#patient?uuid=' + patient.ID);
            $('#instance .study-link').attr('href', '#study?uuid=' + study.ID);
            $('#instance .series-link').attr('href', '#series?uuid=' + series.ID);
            
            $('#instance-info li').remove();
            $('#instance-info')
              .append('<li data-role="list-divider">Patient</li>')
              .append(FormatPatient(patient, '#patient?uuid=' + patient.ID, true))
              .append('<li data-role="list-divider">Study</li>')
              .append(FormatStudy(study, '#study?uuid=' + study.ID, true))
              .append('<li data-role="list-divider">Series</li>')
              .append(FormatSeries(series, '#series?uuid=' + series.ID, true))
              .append('<li data-role="list-divider">Instance</li>')
              .append(FormatInstance(instance))
              .listview('refresh');

            GetResource('/instances/' + instance.ID + '/tags', function(s) {
              $('#dicom-tree').tree('loadData', ConvertForTree(s));
            });

            SetupAnonymizedOrModifiedFrom('#instance-anonymized-from', instance, 'instance', 'AnonymizedFrom');
            SetupAnonymizedOrModifiedFrom('#instance-modified-from', instance, 'instance', 'ModifiedFrom');

            currentPage = 'instance';
            currentUuid = $.mobile.pageData.uuid;
          });
        });
      });
    });
  }
}

$(document).live('pagebeforehide', function() {
  currentPage = '';
  currentUuid = '';
});



$('#patient').live('pagebeforeshow', RefreshPatient);
$('#study').live('pagebeforeshow', RefreshStudy);
$('#series').live('pagebeforeshow', RefreshSeries);
$('#instance').live('pagebeforeshow', RefreshInstance);

$(function() {
  $(window).hashchange(function(e, data) {
    // This fixes the navigation with the back button and with the anonymization
    if ('uuid' in $.mobile.pageData &&
        currentPage == $.mobile.pageData.active &&
        currentUuid != $.mobile.pageData.uuid) {
      Refresh();
    }
  });
});





function DeleteResource(path)
{
  $.ajax({
    url: path,
    type: 'DELETE',
    dataType: 'json',
    async: false,
    success: function(s) {
      var ancestor = s.RemainingAncestor;
      if (ancestor == null)
        $.mobile.changePage('#find-patients');
      else
        $.mobile.changePage('#' + ancestor.Type.toLowerCase() + '?uuid=' + ancestor.ID);
    }
  });
}



function OpenDeleteResourceDialog(path, title)
{
  $(document).simpledialog2({ 
    // http://dev.jtsage.com/jQM-SimpleDialog/demos2/
    // http://dev.jtsage.com/jQM-SimpleDialog/demos2/options.html
    mode: 'button',
    animate: false,
    headerText: title,
    headerClose: true,
    width: '500px',
    buttons : {
      'OK': {
        click: function () { 
          DeleteResource(path);
        },
        icon: "delete",
        theme: "c"
      },
      'Cancel': {
        click: function () { 
        }
      }
    }
  });
}



$('#instance-delete').live('click', function() {
  OpenDeleteResourceDialog('../instances/' + $.mobile.pageData.uuid,
                           'Delete this instance?');
});

$('#study-delete').live('click', function() {
  OpenDeleteResourceDialog('../studies/' + $.mobile.pageData.uuid,
                           'Delete this study?');
});

$('#series-delete').live('click', function() {
  OpenDeleteResourceDialog('../series/' + $.mobile.pageData.uuid,
                           'Delete this series?');
});

$('#patient-delete').live('click', function() {
  OpenDeleteResourceDialog('../patients/' + $.mobile.pageData.uuid,
                           'Delete this patient?');
});


$('#instance-download-dicom').live('click', function(e) {
  // http://stackoverflow.com/a/1296101
  e.preventDefault();  //stop the browser from following
  window.location.href = '../instances/' + $.mobile.pageData.uuid + '/file';
});

$('#instance-download-json').live('click', function(e) {
  // http://stackoverflow.com/a/1296101
  e.preventDefault();  //stop the browser from following
  window.location.href = '../instances/' + $.mobile.pageData.uuid + '/tags';
});



$('#instance-preview').live('click', function(e) {
  if ($.mobile.pageData) {
    var pdf = '../instances/' + $.mobile.pageData.uuid + '/pdf';
    $.ajax({
      url: pdf,
      cache: false,
      success: function(s) {
        window.location.assign(pdf);
      },
      error: function() {
        GetResource('/instances/' + $.mobile.pageData.uuid + '/frames', function(frames) {
          if (frames.length == 1)
          {
            // Viewing a single-frame image
            jQuery.slimbox('../instances/' + $.mobile.pageData.uuid + '/preview', '', {
              overlayFadeDuration : 1,
              resizeDuration : 1,
              imageFadeDuration : 1
            });
          }
          else
          {
            // Viewing a multi-frame image

            var images = [];
            for (var i = 0; i < frames.length; i++) {
              images.push([ '../instances/' + $.mobile.pageData.uuid + '/frames/' + i + '/preview' ]);
            }

            jQuery.slimbox(images, 0, {
              overlayFadeDuration : 1,
              resizeDuration : 1,
              imageFadeDuration : 1,
              loop : true
            });
          }
        });
      }
    });
  }
});



$('#series-preview').live('click', function(e) {
  if ($.mobile.pageData) {
    GetResource('/series/' + $.mobile.pageData.uuid, function(series) {
      GetResource('/series/' + $.mobile.pageData.uuid + '/instances', function(instances) {
        Sort(instances, function(x) { return x.IndexInSeries; }, true, false);

        var images = [];
        for (var i = 0; i < instances.length; i++) {
          images.push([ '../instances/' + instances[i].ID + '/preview',
                        '{0}/{1}'.format(i + 1, instances.length) ])
        }

        jQuery.slimbox(images, 0, {
          overlayFadeDuration : 1,
          resizeDuration : 1,
          imageFadeDuration : 1,
          loop : true
        });
      });
    });
  }
});





function ChooseDicomModality(callback)
{
  var clickedModality = '';
  var clickedPeer = '';
  var items = $('<ul>')
    .attr('data-divider-theme', 'd')
    .attr('data-role', 'listview');

  // Retrieve the list of the known DICOM modalities
  $.ajax({
    url: '../modalities',
    type: 'GET',
    dataType: 'json',
    async: false,
    cache: false,
    success: function(modalities) {
      if (modalities.length > 0)
      {
        items.append('<li data-role="list-divider">DICOM modalities</li>');

        for (var i = 0; i < modalities.length; i++) {
          var name = modalities[i];
          var item = $('<li>')
            .html('<a href="#" rel="close">' + name + '</a>')
            .attr('name', name)
            .click(function() { 
              clickedModality = $(this).attr('name');
            });
          items.append(item);
        }
      }

      // Retrieve the list of the known Orthanc peers
      $.ajax({
        url: '../peers',
        type: 'GET',
        dataType: 'json',
        async: false,
        cache: false,
        success: function(peers) {
          if (peers.length > 0)
          {
            items.append('<li data-role="list-divider">Orthanc peers</li>');

            for (var i = 0; i < peers.length; i++) {
              var name = peers[i];
              var item = $('<li>')
                .html('<a href="#" rel="close">' + name + '</a>')
                .attr('name', name)
                .click(function() { 
                  clickedPeer = $(this).attr('name');
                });
              items.append(item);
            }
          }

          // Launch the dialog
          $('#dialog').simpledialog2({
            mode: 'blank',
            animate: false,
            headerText: 'Choose target',
            headerClose: true,
            forceInput: false,
            width: '100%',
            blankContent: items,
            callbackClose: function() {
              var timer;
              function WaitForDialogToClose() {
                if (!$('#dialog').is(':visible')) {
                  clearInterval(timer);
                  callback(clickedModality, clickedPeer);
                }
              }
              timer = setInterval(WaitForDialogToClose, 100);
            }
          });
        }
      });
    }
  });
}


$('#instance-store,#series-store,#study-store,#patient-store').live('click', function(e) {
  ChooseDicomModality(function(modality, peer) {
    var url;
    var loading;

    if (modality != '')
    {
      url = '../modalities/' + modality + '/store';
      loading = '#dicom-store';
    }

    if (peer != '')
    {
      url = '../peers/' + peer + '/store';
      loading = '#peer-store';
    }

    if (url != '') {
      $.ajax({
        url: url,
        type: 'POST',
        dataType: 'text',
        data: $.mobile.pageData.uuid,
        async: true,  // Necessary to block UI
        beforeSend: function() {
          $.blockUI({ message: $(loading) });
        },
        complete: function(s) {
          $.unblockUI();
        },
        success: function(s) {
        },
        error: function() {
          alert('Error during store');
        }
      });      
    }
  });
});


$('#show-tag-name').live('change', function(e) {
  var checked = e.currentTarget.checked;
  if (checked)
    $('.tag-name').show();
  else
    $('.tag-name').hide();
});


$('#patient-archive').live('click', function(e) {
  e.preventDefault();  //stop the browser from following
  window.location.href = '../patients/' + $.mobile.pageData.uuid + '/archive';
});

$('#study-archive').live('click', function(e) {
  e.preventDefault();  //stop the browser from following
  window.location.href = '../studies/' + $.mobile.pageData.uuid + '/archive';
});

$('#series-archive').live('click', function(e) {
  e.preventDefault();  //stop the browser from following
  window.location.href = '../series/' + $.mobile.pageData.uuid + '/archive';
});


$('#patient-media').live('click', function(e) {
  e.preventDefault();  //stop the browser from following
  window.location.href = '../patients/' + $.mobile.pageData.uuid + '/media';
});

$('#study-media').live('click', function(e) {
  e.preventDefault();  //stop the browser from following
  window.location.href = '../studies/' + $.mobile.pageData.uuid + '/media';
});

$('#series-media').live('click', function(e) {
  e.preventDefault();  //stop the browser from following
  window.location.href = '../series/' + $.mobile.pageData.uuid + '/media';
});



$('#protection').live('change', function(e) {
  var isProtected = e.target.value == "on";
  $.ajax({
    url: '../patients/' + $.mobile.pageData.uuid + '/protected',
    type: 'PUT',
    dataType: 'text',
    data: isProtected ? '1' : '0',
    async: false
  });
});



function OpenAnonymizeResourceDialog(path, title)
{
  $(document).simpledialog2({ 
    mode: 'button',
    animate: false,
    headerText: title,
    headerClose: true,
    width: '500px',
    buttons : {
      'OK': {
        click: function () { 
          $.ajax({
            url: path + '/anonymize',
            type: 'POST',
            data: '{ "Keep" : [ "SeriesDescription", "StudyDescription" ] }',
            dataType: 'json',
            async: false,
            cache: false,
            success: function(s) {
              // The following line does not work...
              //$.mobile.changePage('explorer.html#patient?uuid=' + s.PatientID);

              window.location.assign('explorer.html#patient?uuid=' + s.PatientID);
              //window.location.reload();
            }
          });
        },
        icon: "delete",
        theme: "c"
      },
      'Cancel': {
        click: function () { 
        }
      }
    }
  });
}

$('#instance-anonymize').live('click', function() {
  OpenAnonymizeResourceDialog('../instances/' + $.mobile.pageData.uuid,
                              'Anonymize this instance?');
});

$('#study-anonymize').live('click', function() {
  OpenAnonymizeResourceDialog('../studies/' + $.mobile.pageData.uuid,
                              'Anonymize this study?');
});

$('#series-anonymize').live('click', function() {
  OpenAnonymizeResourceDialog('../series/' + $.mobile.pageData.uuid,
                              'Anonymize this series?');
});

$('#patient-anonymize').live('click', function() {
  OpenAnonymizeResourceDialog('../patients/' + $.mobile.pageData.uuid,
                              'Anonymize this patient?');
});


$('#plugins').live('pagebeforeshow', function() {
  $.ajax({
    url: '../plugins',
    dataType: 'json',
    async: false,
    cache: false,
    success: function(plugins) {
      var target = $('#all-plugins');
      $('li', target).remove();

      plugins.map(function(id) {
        return $.ajax({
          url: '../plugins/' + id,
          dataType: 'json',
          async: false,
          cache: false,
          success: function(plugin) {
            var li = $('<li>');
            var item = li;

            if ('RootUri' in plugin)
            {
              item = $('<a>');
              li.append(item);
              item.click(function() {
                window.open(plugin.RootUri);
              });
            }

            item.append($('<h1>').text(plugin.ID));
            item.append($('<p>').text(plugin.Description));
            item.append($('<span>').addClass('ui-li-count').text(plugin.Version));
            target.append(li);
          }
        });
      });

      target.listview('refresh');
    }
  });
});
