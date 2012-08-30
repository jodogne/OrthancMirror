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



function SortOnDicomTag(arr, tag, isInteger, reverse)
{
  var defaultValue;
  if (isInteger)
    defaultValue = 0;
  else
    defaultValue = '';

  arr.sort(function(a, b) {
    var ta = a.MainDicomTags[tag];
    var tb = b.MainDicomTags[tag];
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



function GetSingleResource(type, uuid, callback)
{
  var resource = null;
  $.ajax({
    url: '/' + type + '/' + uuid,
    dataType: 'json',
    async: false,
    success: function(s) {
      callback(s);
    }
  });
}


function GetMultipleResources(type, uuids, callback)
{
  if (uuids == null)
  {
    $.ajax({
      url: '/' + type,
      dataType: 'json',
      async: false,
      success: function(s) {
        uuids = s;
      }
    });
  }

  var resources = [];
  var ajaxRequests = uuids.map(function(uuid) {
    return $.ajax({
      url: '/' + type + '/' + uuid,
      dataType: 'json',
      async: true,
      success: function(s) {
        resources.push(s);
      }
    });
  });

  // Wait for all the AJAX requests to end
  $.when.apply($, ajaxRequests).then(function() {
    callback(resources);
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



function FormatPatient(patient, link, isReverse)
{
  var s = ('<h3>{1}</h3>' + 
           '<p>Patient ID: <strong>{2}</strong></p>' +
           '<p>Accession Number: <strong>{3}</strong></p>' +
           '<p>Date of Birth: <strong>{4}</strong></p>' +
           '<p>Sex: <strong>{5}</strong></p>' +
           '<span class="ui-li-count">{6}</span>'
          ).format
  (link,
   patient.MainDicomTags.PatientName,
   patient.DicomPatientID,
   patient.MainDicomTags.AccessionNumber,
   FormatDicomDate(patient.MainDicomTags.PatientBirthDate),
   patient.MainDicomTags.PatientSex,
   patient.Studies.length
  );

  return CompleteFormatting(s, link, isReverse);
}



function FormatStudy(study, link, isReverse)
{
  var s = ('<h3>{0}</h3>' +
           '<p>Study Instance UID: <strong>{1}</strong></p>' +
           '<span class="ui-li-count">{2}</span>'
           ).format
  (study.MainDicomTags.StudyDescription,
   SplitLongUid(study.DicomStudyInstanceUID),
   study.Series.length
  );

  return CompleteFormatting(s, link, isReverse);
}



function FormatSeries(series, link, isReverse)
{
  var s = ('<h3>{0}</h3>' +
           '<p>Modality: <strong>{1}</strong></p>' +
           '<p>Protocol: <strong>{2}</strong></p>' +
           '<p>Station name: <strong>{3}</strong></p>' +
           '<p>Series Instance UID: <strong>{4}</strong></p>' +
           '<span class="ui-li-count">{5}</span>').format
  (series.MainDicomTags.SeriesDescription,
   series.MainDicomTags.Modality,
   series.MainDicomTags.ProtocolName,
   series.MainDicomTags.StationName,
   SplitLongUid(series.DicomSeriesInstanceUID),
   series.Instances.length
  );

  return CompleteFormatting(s, link, isReverse);
}


function FormatInstance(instance, link, isReverse)
{
  var s = ('<h3>Instance {0}</h3>' +
           '<p>SOP Instance UID: <strong>{1}</strong></p>'
          ).format
  (instance.MainDicomTags.InstanceNumber,
   instance.DicomSOPInstanceUID
  );

  return CompleteFormatting(s, link, isReverse);
}




$('#find-patients').live('pagebeforeshow', function() {
  GetMultipleResources('patients', null, function(patients) {
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



$('#patient').live('pagebeforeshow', function() {
  if ($.mobile.pageData) {
    GetSingleResource('patients', $.mobile.pageData.uuid, function(patient) {
      GetMultipleResources('studies', patient.Studies, function(studies) {
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

        target.listview('refresh');
      });
    });
  }
});


$('#study').live('pagebeforeshow', function() {
  if ($.mobile.pageData) {
    GetSingleResource('studies', $.mobile.pageData.uuid, function(study) {
      GetSingleResource('patients', study.ParentPatient, function(patient) {
        GetMultipleResources('series', study.Series, function(series) {
          SortOnDicomTag(series, 'SeriesDate', false, true);

          $('#study-info li').remove();
          $('#study-info')
            .append('<li data-role="list-divider">Patient</li>')
            .append(FormatPatient(patient, '#patient?uuid=' + patient.ID, true))
            .append('<li data-role="list-divider">Study</li>')
            .append(FormatStudy(study))
            .listview('refresh');

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
        });
      });  
    });
  }
});
  

$('#series').live('pagebeforeshow', function() {
  if ($.mobile.pageData) {
    GetSingleResource('series', $.mobile.pageData.uuid, function(series) {
      GetSingleResource('studies', series.ParentStudy, function(study) {
        GetSingleResource('patients', study.ParentPatient, function(patient) {
          GetMultipleResources('instances', series.Instances, function(instances) {
            SortOnDicomTag(instances, 'InstanceNumber', true, false);

            $('#series-info li').remove();
            $('#series-info')
              .append('<li data-role="list-divider">Patient</li>')
              .append(FormatPatient(patient, '#patient?uuid=' + patient.ID, true))
              .append('<li data-role="list-divider">Study</li>')
              .append(FormatStudy(study, '#study?uuid=' + study.ID, true))
              .append('<li data-role="list-divider">Series</li>')
              .append(FormatSeries(series))
              .listview('refresh');

            var target = $('#list-instances');
            $('li', target).remove();
            for (var i = 0; i < instances.length; i++) {
              target.append(FormatInstance(instances[i], '#instance?uuid=' + instances[i].ID));
            }
            target.listview('refresh');
          });
        });
      });
    });
  }
});



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


$('#instance').live('pagebeforeshow', function() {
  if ($.mobile.pageData) {
    GetSingleResource('instances', $.mobile.pageData.uuid, function(instance) {
      GetSingleResource('series', instance.ParentSeries, function(series) {
        GetSingleResource('studies', series.ParentStudy, function(study) {
          GetSingleResource('patients', study.ParentPatient, function(patient) {
            
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

            $.ajax({
              url: '/instances/' + instance.ID + '/tags',
              dataType: 'json',
              success: function(s) {
                $('#dicom-tree').tree('loadData', ConvertForTree(s));
              }
            });

          });
        });
      });
    });
  }
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
        $.mobile.changePage('#' + ancestor.Type + '?uuid=' + ancestor.ID);
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
  OpenDeleteResourceDialog('/instances/' + $.mobile.pageData.uuid,
                           'Delete this instance?');
});

$('#study-delete').live('click', function() {
  OpenDeleteResourceDialog('/studies/' + $.mobile.pageData.uuid,
                           'Delete this study?');
});

$('#series-delete').live('click', function() {
  OpenDeleteResourceDialog('/series/' + $.mobile.pageData.uuid,
                           'Delete this series?');
});

$('#patient-delete').live('click', function() {
  OpenDeleteResourceDialog('/patients/' + $.mobile.pageData.uuid,
                           'Delete this patient?');
});


$('#instance-download-dicom').live('click', function(e) {
  // http://stackoverflow.com/a/1296101
  e.preventDefault();  //stop the browser from following
  window.location.href = '/instances/' + $.mobile.pageData.uuid + '/file';
});

$('#instance-download-json').live('click', function(e) {
  // http://stackoverflow.com/a/1296101
  e.preventDefault();  //stop the browser from following
  window.location.href = '/instances/' + $.mobile.pageData.uuid + '/tags';
});


$('#instance-preview').live('click', function(e) {
  jQuery.slimbox('/instances/' + $.mobile.pageData.uuid + '/normalized-image', '', {
                   overlayFadeDuration : 1,
                   resizeDuration : 1,
                   imageFadeDuration : 1
                 });
});

$('#series-preview').live('click', function(e) {
  if ($.mobile.pageData) {
    GetSingleResource('series', $.mobile.pageData.uuid, function(series) {
      GetMultipleResources('instances', series.Instances, function(instances) {
        SortOnDicomTag(instances, 'InstanceNumber', true, false);

        var images = [];
        for (var i = 0; i < instances.length; i++) {
          images.push([ '/instances/' + instances[i].ID + '/normalized-image',
                        '{0}/{1}'.format(i + 1, instances.length) ])
        }

        jQuery.slimbox(images, 0, {
          overlayFadeDuration : 1,
          resizeDuration : 1,
          imageFadeDuration : 1,
          loop : true
        });
      })
    });
  }
});






function ChooseDicomModality(callback)
{
  $.ajax({
    url: '/modalities',
    type: 'GET',
    dataType: 'json',
    async: false,
    success: function(modalities) {
      var clickedModality = '';
      var items = $('<ul>')
        .attr('data-role', 'listview');

      for (var i = 0; i < modalities.length; i++) {
        var modality = modalities[i];
        var item = $('<li>')
          .html('<a href="#" rel="close">' + modality + '</a>')
          .attr('modality', modality)
          .click(function() { 
            clickedModality = $(this).attr('modality');
          });
        items.append(item);
      }

      $('#dialog').simpledialog2({
        mode: 'blank',
        animate: false,
        headerText: 'DICOM modality',
        headerClose: true,
        width: '100%',
        blankContent: items,
        callbackClose: function() {
          var timer;
          function WaitForDialogToClose() {
            if (!$('#dialog').is(':visible')) {
              clearInterval(timer);
              callback(clickedModality);
            }
          }
          timer = setInterval(WaitForDialogToClose, 100);
        }
      });
    }
  });
}


$('#instance-store,#series-store').live('click', function(e) {
  ChooseDicomModality(function(modality) {
    if (modality != '') {
      $.ajax({
        url: '/modalities/' + modality + '/store',
        type: 'POST',
        dataType: 'text',
        data: $.mobile.pageData.uuid,
        async: true,  // Necessary to block UI
        beforeSend: function() {
          $.blockUI({ message: $('#loading') });
        },
        complete: function(s) {
          $.unblockUI();
        },
        success: function(s) {
          console.log('done !');
        },
        error: function() {
          alert('Error during C-Store');
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
