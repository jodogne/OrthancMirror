function JavascriptDateToDicom(date)
{
  var s = date.toISOString();
  return s.substring(0, 4) + s.substring(5, 7) + s.substring(8, 10);
}

function GenerateDicomDate(days)
{
  var today = new Date();
  var other = new Date(today);
  other.setDate(today.getDate() + days);
  return JavascriptDateToDicom(other);
}


$('#query-retrieve').live('pagebeforeshow', function() {
  $.ajax({
    url: '../modalities',
    dataType: 'json',
    async: false,
    cache: false,
    success: function(modalities) {
      var target = $('#qr-server');
      $('option', target).remove();

      for (var i = 0; i < modalities.length; i++) {
        var option = $('<option>').text(modalities[i]);
        target.append(option);
      }

      target.selectmenu('refresh');
    }
  });

  var target = $('#qr-date');
  $('option', target).remove();
  target.append($('<option>').attr('value', '*').text('Any date'));
  target.append($('<option>').attr('value', GenerateDicomDate(0)).text('Today'));
  target.append($('<option>').attr('value', GenerateDicomDate(-1)).text('Yesterday'));
  target.append($('<option>').attr('value', GenerateDicomDate(-7) + '-').text('Last 7 days'));
  target.append($('<option>').attr('value', GenerateDicomDate(-31) + '-').text('Last 31 days'));
  target.append($('<option>').attr('value', GenerateDicomDate(-31 * 3) + '-').text('Last 3 months'));
  target.append($('<option>').attr('value', GenerateDicomDate(-365) + '-').text('Last year'));
  target.selectmenu('refresh');
});


$('#qr-echo').live('click', function() {
  var server = $('#qr-server').val();
  var message = 'Error: The C-Echo has failed!';

  $.ajax({
    url: '../modalities/' + server + '/echo',
    type: 'POST', 
    cache: false,
    async: false,
    success: function() {
      message = 'The C-Echo has succeeded!';
    }
  });

  $('<div>').simpledialog2({
    mode: 'button',
    headerText: 'Echo result',
    headerClose: true,
    buttonPrompt: message,
    animate: false,
    buttons : {
      'OK': { click: function () { } }
    }
  });

  return false;
});


$('#qr-submit').live('click', function() {
  var query = {
    'Level' : 'Study',
    'Query' : {
      'AccessionNumber' : '*',
      'PatientBirthDate' : '*',
      'PatientID' : '*',
      'PatientName' : '*',
      'PatientSex' : '*',
      'SpecificCharacterSet' : 'ISO_IR 192',  // UTF-8
      'StudyDate' : $('#qr-date').val(),
      'StudyDescription' : '*'
    }
  };

  var field = $('#qr-fields input:checked').val();
  query['Query'][field] = $('#qr-value').val().toUpperCase();

  var modalities = '';
  $('#qr-modalities input:checked').each(function() {
    var s = $(this).attr('name');
    if (modalities == '*')
      modalities = s;
    else
      modalities += '\\' + s;
  });

  if (modalities.length > 0) {
    query['Query']['ModalitiesInStudy'] = modalities;
  }


  var server = $('#qr-server').val();
  $.ajax({
    url: '../modalities/' + server + '/query',
    type: 'POST', 
    data: JSON.stringify(query),
    dataType: 'json',
    async: false,
    error: function() {
      alert('Error during query (C-Find)');
    },
    success: function(result) {
      window.location.assign('explorer.html#query-retrieve-2?server=' + server + '&uuid=' + result['ID']);
    }
  });

  return false;
});



function Retrieve(url)
{
  $.ajax({
    url: '../system',
    dataType: 'json',
    async: false,
    success: function(system) {
      $('<div>').simpledialog2({
        mode: 'button',
        headerText: 'Target',
        headerClose: true,
        buttonPrompt: 'Enter Application Entity Title (AET):',
        buttonInput: true,
        buttonInputDefault: system['DicomAet'],
        buttons : {
          'OK': {
            click: function () { 
              var aet = $.mobile.sdLastInput;
              if (aet.length == 0)
                aet = system['DicomAet'];

              $.ajax({
                url: url,
                type: 'POST',
                async: true,  // Necessary to block UI
                dataType: 'text',
                data: aet,
                beforeSend: function() {
                  $.blockUI({ message: $('#info-retrieve') });
                },
                complete: function(s) {
                  $.unblockUI();
                },
                error: function() {
                  alert('Error during retrieve');
                }
              });
            }
          }
        }
      });
    }
  });
}




$('#query-retrieve-2').live('pagebeforeshow', function() {
  if ($.mobile.pageData) {
    var uri = '../queries/' + $.mobile.pageData.uuid + '/answers';

    $.ajax({
      url: uri,
      dataType: 'json',
      async: false,
      success: function(answers) {
        var target = $('#query-retrieve-2 ul');
        $('li', target).remove();

        for (var i = 0; i < answers.length; i++) {
          $.ajax({
            url: uri + '/' + answers[i] + '/content?simplify',
            dataType: 'json',
            async: false,
            success: function(study) {
              var series = '#query-retrieve-3?server=' + $.mobile.pageData.server + '&uuid=' + study['StudyInstanceUID'];
              var info = $('<a>').attr('href', series).html(
                ('<h3>{0} - {1}</h3>' + 
                 '<p>Accession number: <b>{2}</b></p>' +
                 '<p>Birth date: <b>{3}</b></p>' +
                 '<p>Patient sex: <b>{4}</b></p>' +
                 '<p>Study description: <b>{5}</b></p>' +
                 '<p>Study date: <b>{6}</b></p>').format(
                   study['PatientID'],
                   study['PatientName'],
                   study['AccessionNumber'],
                   FormatDicomDate(study['PatientBirthDate']),
                   study['PatientSex'],
                   study['StudyDescription'],
                   FormatDicomDate(study['StudyDate'])));

              var studyUri = uri + '/' + answers[i] + '/retrieve';
              var retrieve = $('<a>').text('Retrieve').click(function() {
                Retrieve(studyUri);
              });

              target.append($('<li>').append(info).append(retrieve));
            }
          });
        }

        target.listview('refresh');
      }
    });
  }
});


$('#query-retrieve-3').live('pagebeforeshow', function() {
  if ($.mobile.pageData) {
    var query = {
      'Level' : 'Series',
      'Query' : {
        'Modality' : '*',
        'ProtocolName' : '*',
        'SeriesDescription' : '*',
        'SeriesInstanceUID' : '*',
        'StudyInstanceUID' : $.mobile.pageData.uuid
      }
    };

    $.ajax({
      url: '../modalities/' + $.mobile.pageData.server + '/query',
      type: 'POST', 
      data: JSON.stringify(query),
      dataType: 'json',
      async: false,
      error: function() {
        alert('Error during query (C-Find)');
      },
      success: function(answer) {
        var uri = '../queries/' + answer['ID'] + '/answers';

        $.ajax({
          url: uri,
          dataType: 'json',
          async: false,
          success: function(answers) {
            
            var target = $('#query-retrieve-3 ul');
            $('li', target).remove();

            for (var i = 0; i < answers.length; i++) {
              $.ajax({
                url: uri + '/' + answers[i] + '/content?simplify',
                dataType: 'json',
                async: false,
                success: function(series) {
                  var info = $('<a>').html(
                    ('<h3>{0}</h3>'  + 
                     '<p>Modality: <b>{1}</b></p>' +
                     '<p>Protocol name: <b>{2}</b></p>'
                    ).format(
                      series['SeriesDescription'],
                      series['Modality'],
                      series['ProtocolName']
                    ));

                  var seriesUri = uri + '/' + answers[i] + '/retrieve';
                  var retrieve = $('<a>').text('Retrieve').click(function() {
                    Retrieve(seriesUri);
                  });

                  target.append($('<li>').append(info).append(retrieve));
                }
              });
            }

            target.listview('refresh');
          }
        });
      }
    });
  }
});
