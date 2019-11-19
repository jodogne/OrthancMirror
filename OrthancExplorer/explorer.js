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


var LIMIT_RESOURCES = 100;

var currentPage = '';
var currentUuid = '';


function DeepCopy(obj)
{
  return jQuery.extend(true, {}, obj);
}


function ChangePage(page, options)
{
  var first = true;
  var value;

  if (options) {
    for (var key in options) {
      value = options[key];
      if (first) {
        page += '?';
        first = false;
      } else {
        page += '&';
      }
      
      page += key + '=' + value;
    }
  }

  window.location.replace('explorer.html#' + page);
  /*$.mobile.changePage('#' + page, {
    changeHash: true
  });*/
}


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

  // Inject the template of the warning about insecure setup as the
  // first child of each page
  var insecure = $('#template-insecure').html();
  $('[data-role="page"]>[data-role="content"]').prepend(insecure);
  
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

function GetAuthorizationTokensFromUrl() {
  var urlVariables = window.location.search.substring(1).split('&');
  var dict = {};

  for (var i = 0; i < urlVariables.length; i++) {
      var split = urlVariables[i].split('=');

      if (split.length == 2 && (split[0] == "token" || split[0] == "auth-token" || split[0] == "authorization")) {
        dict[split[0]] = split[1];
      }
  }
  return dict;
};

var authorizationTokens = GetAuthorizationTokensFromUrl();

/* Copy the authoziation toekn from the url search parameters into HTTP headers in every request to the Rest API.  
Thanks to this behaviour, you may specify a ?token=xxx in your url and this will be passed 
as the "token" header in every request to the API allowing you to use the authorization plugin */
$.ajaxSetup(
  {
    headers : authorizationTokens
  }
);


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

function FormatFloatSequence(s)
{
  if (s == undefined || s.length == 0)
    return "-";

  if (s.indexOf("\\") == -1)
    return s;

  var oldValues = s.split("\\");
  var newValues = [];
  for (var i = 0; i < oldValues.length; i++)
  {
    newValues.push(parseFloat(oldValues[i]).toFixed(3));
  }
  return newValues.join("\\");
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


function CompleteFormatting(node, link, isReverse, count)
{
  if (count != null)
  {
    node = node.add($('<span>')
                    .addClass('ui-li-count')
                    .text(count));
  }
  
  if (link != null)
  {
    node = $('<a>').attr('href', link).append(node);

    if (isReverse)
      node.attr('data-direction', 'reverse')
  }

  node = $('<li>').append(node);

  if (isReverse)
    node.attr('data-icon', 'back');

  return node;
}


function FormatMainDicomTags(target, tags, tagsToIgnore)
{
  var v;
  
  for (var i in tags)
  {
    if (tagsToIgnore.indexOf(i) == -1)
    {
      v = tags[i];

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
      else if (i == "ImagePositionPatient" ||
               i == "ImageOrientationPatient")
      {
        v = FormatFloatSequence(v);
      }
      
      target.append($('<p>')
                    .text(i + ': ')
                    .append($('<strong>').text(v)));
    }
  }
}


function FormatPatient(patient, link, isReverse)
{
  var node = $('<div>').append($('<h3>').text(patient.MainDicomTags.PatientName));

  FormatMainDicomTags(node, patient.MainDicomTags, [ 
    "PatientName"
    // "OtherPatientIDs"
  ]);
    
  return CompleteFormatting(node, link, isReverse, patient.Studies.length);
}



function FormatStudy(study, link, isReverse, includePatient)
{
  var label;
  var node;

  if (includePatient) {
    label = study.Label;
  } else {
    label = study.MainDicomTags.StudyDescription;
  }

  node = $('<div>').append($('<h3>').text(label));

  if (includePatient) {
    FormatMainDicomTags(node, study.PatientMainDicomTags, [ 
      'PatientName'
    ]);
  }
    
  FormatMainDicomTags(node, study.MainDicomTags, [ 
     'StudyDescription', 
     'StudyTime' 
  ]);

  return CompleteFormatting(node, link, isReverse, study.Series.length);
}



function FormatSeries(series, link, isReverse)
{
  var c;
  var node;

  if (series.ExpectedNumberOfInstances == null ||
      series.Instances.length == series.ExpectedNumberOfInstances)
  {
    c = series.Instances.length;
  }
  else
  {
    c = series.Instances.length + '/' + series.ExpectedNumberOfInstances;
  }
  
  node = $('<div>')
      .append($('<h3>').text(series.MainDicomTags.SeriesDescription))
      .append($('<p>').append($('<em>')
                           .text('Status: ')
                           .append($('<strong>').text(series.Status))));

  FormatMainDicomTags(node, series.MainDicomTags, [ 
     "SeriesDescription", 
     "SeriesTime", 
     "Manufacturer",
     "ImagesInAcquisition",
     "SeriesDate",
     "ImageOrientationPatient"
  ]);
    
  return CompleteFormatting(node, link, isReverse, c);
}


function FormatInstance(instance, link, isReverse)
{
  var node = $('<div>').append($('<h3>').text('Instance: ' + instance.IndexInSeries));

  FormatMainDicomTags(node, instance.MainDicomTags, [
    "AcquisitionNumber", 
    "InstanceNumber", 
    "InstanceCreationDate", 
    "InstanceCreationTime"
  ]);
    
  return CompleteFormatting(node, link, isReverse);
}


$('[data-role="page"]').live('pagebeforeshow', function() {
  $.ajax({
    url: '../system',
    dataType: 'json',
    async: false,
    cache: false,
    success: function(s) {
      if (s.Name != "") {
        $('.orthanc-name').html($('<a>')
                                .addClass('ui-link')
                                .attr('href', 'explorer.html')
                                .text(s.Name)
                                .append(' &raquo; '));
      }

      // New in Orthanc 1.5.8
      if ('IsHttpServerSecure' in s &&
          !s.IsHttpServerSecure) {
        $('.warning-insecure').show();
      } else {
        $('.warning-insecure').hide();
      }
    }
  });
});



$('#lookup').live('pagebeforeshow', function() {
  // NB: "GenerateDicomDate()" is defined in "query-retrieve.js"
  var target = $('#lookup-study-date');
  $('option', target).remove();
  target.append($('<option>').attr('value', '*').text('Any date'));
  target.append($('<option>').attr('value', GenerateDicomDate(0)).text('Today'));
  target.append($('<option>').attr('value', GenerateDicomDate(-1)).text('Yesterday'));
  target.append($('<option>').attr('value', GenerateDicomDate(-7) + '-').text('Last 7 days'));
  target.append($('<option>').attr('value', GenerateDicomDate(-31) + '-').text('Last 31 days'));
  target.append($('<option>').attr('value', GenerateDicomDate(-31 * 3) + '-').text('Last 3 months'));
  target.append($('<option>').attr('value', GenerateDicomDate(-365) + '-').text('Last year'));
  target.selectmenu('refresh');

  $('#lookup-result').hide();
});


$('#lookup-submit').live('click', function() {
  var lookup;

  $('#lookup-result').hide();

  lookup = {
    'Level' : 'Study',
    'Expand' : true,
    'Limit' : LIMIT_RESOURCES + 1,
    'Query' : {
      'StudyDate' : $('#lookup-study-date').val()
    }
  };

  $('#lookup-form input').each(function(index, input) {
    if (input.value.length != 0) {
      if (input.id == 'lookup-patient-id') {
        lookup['Query']['PatientID'] = input.value;
      } 
      else if (input.id == 'lookup-patient-name') {
        lookup['Query']['PatientName'] = input.value;
      } 
      else if (input.id == 'lookup-accession-number') {
        lookup['Query']['AccessionNumber'] = input.value;
      } 
      else if (input.id == 'lookup-study-description') {
        lookup['Query']['StudyDescription'] = input.value;
      }
      else {
        console.error('Unknown lookup field: ' + input.id);
      }
    } 
  });

  $.ajax({
    url: '../tools/find',
    type: 'POST', 
    data: JSON.stringify(lookup),
    dataType: 'json',
    async: false,
    error: function() {
      alert('Error during lookup');
    },
    success: function(studies) {
      FormatListOfStudies('#lookup-result ul', '#lookup-alert', '#lookup-count', studies);
      $('#lookup-result').show();
    }
  });

  return false;
});


$('#find-patients').live('pagebeforeshow', function() {
  GetResource('/patients?expand&since=0&limit=' + (LIMIT_RESOURCES + 1), function(patients) {
    var target = $('#all-patients');
    var count, showAlert, p;


    $('li', target).remove();
    
    SortOnDicomTag(patients, 'PatientName', false, false);

    if (patients.length <= LIMIT_RESOURCES) {
      count = patients.length;
      showAlert = false;
    }
    else {
      count = LIMIT_RESOURCES;
      showAlert = true;
    }

    for (var i = 0; i < count; i++) {
      p = FormatPatient(patients[i], '#patient?uuid=' + patients[i].ID);
      target.append(p);
    }

    target.listview('refresh'); 

    if (showAlert) {
      $('#count-patients').text(LIMIT_RESOURCES);
      $('#alert-patients').show();
    } else {
      $('#alert-patients').hide();
    }
  });
});



function FormatListOfStudies(targetId, alertId, countId, studies)
{
  var target = $(targetId);
  var patient, study, s;
  var count, showAlert;

  $('li', target).remove();

  for (var i = 0; i < studies.length; i++) {
    patient = studies[i].PatientMainDicomTags.PatientName;
    study = studies[i].MainDicomTags.StudyDescription;

    s = "";
    if (typeof patient === 'string') {
      s = patient;
    }

    if (typeof study === 'string') {
      if (s.length > 0) {
        s += ' - ';
      }

      s += study;
    }

    studies[i]['Label'] = s;
  }

  Sort(studies, function(a) { return a.Label }, false, false);

  if (studies.length <= LIMIT_RESOURCES) {
    count = studies.length;
    showAlert = false;
  }
  else {
    count = LIMIT_RESOURCES;
    showAlert = true;
  }

  for (var i = 0; i < count; i++) {
    s = FormatStudy(studies[i], '#study?uuid=' + studies[i].ID, false, true);
    target.append(s);
  }

  target.listview('refresh');

  if (showAlert) {
    $(countId).text(LIMIT_RESOURCES);
    $(alertId).show();
  } else {
    $(alertId).hide();
  }
}


$('#find-studies').live('pagebeforeshow', function() {
  GetResource('/studies?expand&since=0&limit=' + (LIMIT_RESOURCES + 1), function(studies) {
    FormatListOfStudies('#all-studies', '#alert-studies', '#count-studies', studies);
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
  var pageData, target, v;

  if ($.mobile.pageData) {
    pageData = DeepCopy($.mobile.pageData);

    GetResource('/patients/' + pageData.uuid, function(patient) {
      GetResource('/patients/' + pageData.uuid + '/studies', function(studies) {
        SortOnDicomTag(studies, 'StudyDate', false, true);

        $('#patient-info li').remove();
        $('#patient-info')
          .append('<li data-role="list-divider">Patient</li>')
          .append(FormatPatient(patient))
          .listview('refresh');

        target = $('#list-studies');
        $('li', target).remove();
        
        for (var i = 0; i < studies.length; i++) {
          if (i == 0 || studies[i].MainDicomTags.StudyDate != studies[i - 1].MainDicomTags.StudyDate)
          {
            target.append($('<li>')
                          .attr('data-role', 'list-divider')
                          .text(FormatDicomDate(studies[i].MainDicomTags.StudyDate)));
          }

          target.append(FormatStudy(studies[i], '#study?uuid=' + studies[i].ID));
        }

        SetupAnonymizedOrModifiedFrom('#patient-anonymized-from', patient, 'patient', 'AnonymizedFrom');
        SetupAnonymizedOrModifiedFrom('#patient-modified-from', patient, 'patient', 'ModifiedFrom');

        target.listview('refresh');

        // Check whether this patient is protected
        $.ajax({
          url: '../patients/' + pageData.uuid + '/protected',
          type: 'GET',
          dataType: 'text',
          async: false,
          cache: false,
          success: function (s) {
            v = (s == '1') ? 'on' : 'off';
            $('#protection').val(v).slider('refresh');
          }
        });

        currentPage = 'patient';
        currentUuid = pageData.uuid;
      });
    });
  }
}


function RefreshStudy()
{
  var pageData, target;

  if ($.mobile.pageData) {
    pageData = DeepCopy($.mobile.pageData);

    GetResource('/studies/' + pageData.uuid, function(study) {
      GetResource('/patients/' + study.ParentPatient, function(patient) {
        GetResource('/studies/' + pageData.uuid + '/series', function(series) {
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

          target = $('#list-series');
          $('li', target).remove();
          for (var i = 0; i < series.length; i++) {
            if (i == 0 || series[i].MainDicomTags.SeriesDate != series[i - 1].MainDicomTags.SeriesDate)
            {
              target.append($('<li>')
                            .attr('data-role', 'list-divider')
                            .text(FormatDicomDate(series[i].MainDicomTags.SeriesDate)));
            }
            
            target.append(FormatSeries(series[i], '#series?uuid=' + series[i].ID));
          }
          target.listview('refresh');

          currentPage = 'study';
          currentUuid = pageData.uuid;
        });
      });
    });
  }
}
  

function RefreshSeries() 
{
  var pageData, target;

  if ($.mobile.pageData) {
    pageData = DeepCopy($.mobile.pageData);

    GetResource('/series/' + pageData.uuid, function(series) {
      GetResource('/studies/' + series.ParentStudy, function(study) {
        GetResource('/patients/' + study.ParentPatient, function(patient) {
          GetResource('/series/' + pageData.uuid + '/instances', function(instances) {
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

            target = $('#list-instances');
            $('li', target).remove();
            for (var i = 0; i < instances.length; i++) {
              target.append(FormatInstance(instances[i], '#instance?uuid=' + instances[i].ID));
            }
            target.listview('refresh');

            currentPage = 'series';
            currentUuid = pageData.uuid;
          });
        });
      });
    });
  }
}


function EscapeHtml(value)
{
  var ENTITY_MAP = {
    '&': '&amp;',
    '<': '&lt;',
    '>': '&gt;',
    '"': '&quot;',
    "'": '&#39;',
    '/': '&#x2F;',
    '`': '&#x60;',
    '=': '&#x3D;'
  };

  return String(value).replace(/[&<>"'`=\/]/g, function (s) {
    return ENTITY_MAP[s];
  });
}


function ConvertForTree(dicom)
{
  var result = [];
  var label, c;

  for (var i in dicom) {
    if (dicom[i] != null) {
      label = (i + '<span class="tag-name"> (<i>' +
                   EscapeHtml(dicom[i]["Name"]) +
                   '</i>)</span>: ');

      if (dicom[i]["Type"] == 'String')
      {
        result.push({
          label: label + '<strong>' + EscapeHtml(dicom[i]["Value"]) + '</strong>',
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
        c = [];
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
  var pageData;

  if ($.mobile.pageData) {
    pageData = DeepCopy($.mobile.pageData);

    GetResource('/instances/' + pageData.uuid, function(instance) {
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
            currentUuid = pageData.uuid;
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
        $.mobile.changePage('#lookup');
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
  var pageData, pdf, images;

  if ($.mobile.pageData) {
    pageData = DeepCopy($.mobile.pageData);

    pdf = '../instances/' + pageData.uuid + '/pdf';
    $.ajax({
      url: pdf,
      cache: false,
      success: function(s) {
        window.location.assign(pdf);
      },
      error: function() {
        GetResource('/instances/' + pageData.uuid + '/frames', function(frames) {
          if (frames.length == 1)
          {
            // Viewing a single-frame image
            jQuery.slimbox('../instances/' + pageData.uuid + '/preview', '', {
              overlayFadeDuration : 1,
              resizeDuration : 1,
              imageFadeDuration : 1
            });
          }
          else
          {
            // Viewing a multi-frame image

            images = [];
            for (var i = 0; i < frames.length; i++) {
              images.push([ '../instances/' + pageData.uuid + '/frames/' + i + '/preview' ]);
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
  var pageData, images;

  if ($.mobile.pageData) {
    pageData = DeepCopy($.mobile.pageData);

    GetResource('/series/' + pageData.uuid, function(series) {
      GetResource('/series/' + pageData.uuid + '/instances', function(instances) {
        Sort(instances, function(x) { return x.IndexInSeries; }, true, false);

        images = [];
        for (var i = 0; i < instances.length; i++) {
          images.push([ '../instances/' + instances[i].ID + '/preview',
                        (i + 1).toString() + '/' + instances.length.toString() ])
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
      var name, item;
      
      if (modalities.length > 0)
      {
        items.append('<li data-role="list-divider">DICOM modalities</li>');

        for (var i = 0; i < modalities.length; i++) {
          name = modalities[i];
          item = $('<li>')
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
          var name, item;

          if (peers.length > 0)
          {
            items.append('<li data-role="list-divider">Orthanc peers</li>');

            for (var i = 0; i < peers.length; i++) {
              name = peers[i];
              item = $('<li>')
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
    var pageData = DeepCopy($.mobile.pageData);
    var url, loading;

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
        data: pageData.uuid,
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



function ParseJobTime(s)
{
  var t = (s.substr(0, 4) + '-' +
           s.substr(4, 2) + '-' +
           s.substr(6, 5) + ':' +
           s.substr(11, 2) + ':' +
           s.substr(13));
  var utc = new Date(t);

  // Convert from UTC to local time
  return new Date(utc.getTime() - utc.getTimezoneOffset() * 60000);
}


function AddJobField(target, description, field)
{
  if (!(typeof field === 'undefined')) {
    target.append($('<p>')
                  .text(description)
                  .append($('<strong>').text(field)));
  }
}


function AddJobDateField(target, description, field)
{
  if (!(typeof field === 'undefined')) {
    target.append($('<p>')
                  .text(description)
                  .append($('<strong>').text(ParseJobTime(field))));
  }
}


$('#jobs').live('pagebeforeshow', function() {
  $.ajax({
    url: '../jobs?expand',
    dataType: 'json',
    async: false,
    cache: false,
    success: function(jobs) {
      var target = $('#all-jobs');
      var running, pending, inactive;

      $('li', target).remove();

      running = $('<li>')
          .attr('data-role', 'list-divider')
          .text('Currently running');

      pending = $('<li>')
          .attr('data-role', 'list-divider')
          .text('Pending jobs');

      inactive = $('<li>')
          .attr('data-role', 'list-divider')
          .text('Inactive jobs');

      target.append(running);
      target.append(pending);
      target.append(inactive);

      jobs.map(function(job) {
        var li = $('<li>');
        var item = $('<a>');
        
        li.append(item);
        item.attr('href', '#job?uuid=' + job.ID);
        item.append($('<h1>').text(job.Type));
        item.append($('<span>').addClass('ui-li-count').text(job.State));
        AddJobField(item, 'ID: ', job.ID);
        AddJobField(item, 'Local AET: ', job.Content.LocalAet);
        AddJobField(item, 'Remote AET: ', job.Content.RemoteAet);
        AddJobDateField(item, 'Creation time: ', job.CreationTime);
        AddJobDateField(item, 'Completion time: ', job.CompletionTime);
        AddJobDateField(item, 'ETA: ', job.EstimatedTimeOfArrival);

        if (job.State == 'Running' ||
            job.State == 'Pending' ||
            job.State == 'Paused') {
          AddJobField(item, 'Priority: ', job.Priority);
          AddJobField(item, 'Progress: ', job.Progress);
        }
        
        if (job.State == 'Running') {
          li.insertAfter(running);
        } else if (job.State == 'Pending' ||
                   job.State == 'Paused') {
          li.insertAfter(pending);
        } else {
          li.insertAfter(inactive);
        }
      });

      target.listview('refresh');
    }
  });
});


$('#job').live('pagebeforeshow', function() {
  var pageData, target;

  if ($.mobile.pageData) {
    pageData = DeepCopy($.mobile.pageData);

    $.ajax({
      url: '../jobs/' + pageData.uuid,
      dataType: 'json',
      async: false,
      cache: false,
      success: function(job) {
        var block, value;
        
        target = $('#job-info');
        $('li', target).remove();

        target.append($('<li>')
                      .attr('data-role', 'list-divider')
                      .text('General information about the job'));

        {                       
          block = $('<li>');
          for (var i in job) {
            if (i == 'CreationTime' ||
                i == 'CompletionTime' ||
                i == 'EstimatedTimeOfArrival') {
              AddJobDateField(block, i + ': ', job[i]);
            } else if (i != 'InternalContent' &&
                      i != 'Content' &&
                      i != 'Timestamp') {
              AddJobField(block, i + ': ', job[i]);
            }
          }
        }

        target.append(block);
        
        target.append($('<li>')
                      .attr('data-role', 'list-divider')
                      .text('Detailed information'));

        {
          block = $('<li>');

          for (var item in job.Content) {
            var value = job.Content[item];
            if (typeof value !== 'string') {
              value = JSON.stringify(value);
            }
            
            AddJobField(block, item + ': ', value);
          }
        }
        
        target.append(block);
        
        target.listview('refresh');

        $('#job-cancel').closest('.ui-btn').hide();
        $('#job-resubmit').closest('.ui-btn').hide();
        $('#job-pause').closest('.ui-btn').hide();
        $('#job-resume').closest('.ui-btn').hide();

        if (job.State == 'Running' ||
            job.State == 'Pending' ||
            job.State == 'Retry') {
          $('#job-cancel').closest('.ui-btn').show();
          $('#job-pause').closest('.ui-btn').show();
        }
        else if (job.State == 'Success') {
        }
        else if (job.State == 'Failure') {
          $('#job-resubmit').closest('.ui-btn').show();
        }
        else if (job.State == 'Paused') {
          $('#job-resume').closest('.ui-btn').show();
        }
      }
    });
  }
});



function TriggerJobAction(action)
{
  $.ajax({
    url: '../jobs/' + $.mobile.pageData.uuid + '/' + action,
    type: 'POST',
    async: false,
    cache: false,
    complete: function(s) {
      window.location.reload();
    }
  });
}

$('#job-cancel').live('click', function() {
  TriggerJobAction('cancel');
});

$('#job-resubmit').live('click', function() {
  TriggerJobAction('resubmit');
});

$('#job-pause').live('click', function() {
  TriggerJobAction('pause');
});

$('#job-resume').live('click', function() {
  TriggerJobAction('resume');
});
