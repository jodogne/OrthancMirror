function JavascriptDateToDicom(date)
{
  let s = date.toISOString();
  return s.substring(0, 4) + s.substring(5, 7) + s.substring(8, 10);
}

function GenerateDicomDate(days)
{
  let today = new Date();
  let other = new Date(today);
  other.setDate(today.getDate() + days);
  return JavascriptDateToDicom(other);
}


$(document).on("pagebeforeshow", "#query-retrieve", function() {
  $.ajax({
    url: '../modalities',
    dataType: 'json',
    async: false,
    cache: false,
    success: function(modalities) {
      let target = $('#qr-server');
      $('option', target).remove();

      for (let i = 0; i < modalities.length; i++) {
        let option = $('<option>').text(modalities[i]);
        target.append(option);
      }

      target.selectmenu('refresh');
    }
  });

  let target = $('#qr-date');
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


$(document).on("click", "#qr-echo", function(e) {
  let server = $('#qr-server').val();
  let message = 'Error: The C-Echo has failed!';

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


$(document).on("click", "#qr-submit", function(e) {
  let query = {
    'Level' : 'Study',
    'Query' : {
      'AccessionNumber' : '*',
      'PatientBirthDate' : '*',
      'PatientID' : '*',
      'PatientName' : '*',
      'PatientSex' : '*',
      'StudyDate' : $('#qr-date').val(),
      'StudyDescription' : '*'
    }
  };

  let field = $('#qr-fields input:checked').val();
  query['Query'][field] = $('#qr-value').val().toUpperCase();

  let modalities = '';
  $('#qr-modalities input:checked').each(function() {
    let s = $(this).attr('name');
    if (modalities == '')
      modalities = s;
    else
      modalities += '\\' + s;
  });

  if (modalities.length > 0) {
    query['Query']['ModalitiesInStudy'] = modalities;
  }


  let server = $('#qr-server').val();
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
      ChangePage('query-retrieve-2', {
        'server' : server,
        'uuid' : result['ID']
      });
    }
  });

  return false;
});



$(document).on("pagebeforeshow", "#query-retrieve-2", function() {
  if ($.mobile.pageData) {
    let pageData = DeepCopy($.mobile.pageData);

    let uri = '../queries/' + pageData.uuid + '/answers';

    $.ajax({
      url: uri,
      dataType: 'json',
      async: false,
      success: function(answers) {
        let target = $('#query-retrieve-2 ul');
        $('li', target).remove();

        for (let i = 0; i < answers.length; i++) {
          $.ajax({
            url: uri + '/' + answers[i] + '/content?simplify',
            dataType: 'json',
            async: false,
            success: function(study) {
              let series = '#query-retrieve-3?server=' + pageData.server + '&uuid=' + study['StudyInstanceUID'];

              let content = ($('<div>')
                             .append($('<h3>').text(study['PatientID'] + ' - ' + study['PatientName']))
                             .append($('<p>').text('Accession number: ')
                                     .append($('<b>').text(study['AccessionNumber'])))
                             .append($('<p>').text('Birth date: ')
                                     .append($('<b>').text(study['PatientBirthDate'])))
                             .append($('<p>').text('Patient sex: ')
                                     .append($('<b>').text(study['PatientSex'])))
                             .append($('<p>').text('Study description: ')
                                     .append($('<b>').text(study['StudyDescription'])))
                             .append($('<p>').text('Study date: ')
                                     .append($('<b>').text(FormatDicomDate(study['StudyDate'])))));

              let info = $('<a>').attr('href', series).html(content);
              
              let answerId = answers[i];
              let retrieve = $('<a>').text('Retrieve all study').click(function() {
                ChangePage('query-retrieve-4', {
                  'query' : pageData.uuid,
                  'answer' : answerId,
                  'server' : pageData.server
                });
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


$(document).on("pagebeforeshow", "#query-retrieve-3", function() {
  if ($.mobile.pageData) {
    let pageData = DeepCopy($.mobile.pageData);

    let query = {
      'Level' : 'Series',
      'Query' : {
        'Modality' : '*',
        'ProtocolName' : '*',
        'SeriesDescription' : '*',
        'SeriesInstanceUID' : '*',
        'StudyInstanceUID' : pageData.uuid
      }
    };

    $.ajax({
      url: '../modalities/' + pageData.server + '/query',
      type: 'POST', 
      data: JSON.stringify(query),
      dataType: 'json',
      async: false,
      error: function() {
        alert('Error during query (C-Find)');
      },
      success: function(answer) {
        let queryUuid = answer['ID'];
        let uri = '../queries/' + answer['ID'] + '/answers';

        $.ajax({
          url: uri,
          dataType: 'json',
          async: false,
          success: function(answers) {
            
            let target = $('#query-retrieve-3 ul');
            $('li', target).remove();

            for (let i = 0; i < answers.length; i++) {
              $.ajax({
                url: uri + '/' + answers[i] + '/content?simplify',
                dataType: 'json',
                async: false,
                success: function(series) {
                  let content = ($('<div>')
                                 .append($('<h3>').text(series['SeriesDescription']))
                                 .append($('<p>').text('Modality: ')
                                         .append($('<b>').text(series['Modality'])))
                                 .append($('<p>').text('ProtocolName: ')
                                         .append($('<b>').text(series['ProtocolName']))));

                  let info = $('<a>').html(content);

                  let answerId = answers[i];
                  info.click(function() {
                    ChangePage('query-retrieve-4', {
                      'query' : queryUuid,
                      'study' : pageData.uuid,
                      'answer' : answerId,
                      'server' : pageData.server
                    });
                  });

                  target.append($('<li>').attr('data-icon', 'arrow-d').append(info));
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



$(document).on("pagebeforeshow", "#query-retrieve-4", function() {
  if ($.mobile.pageData) {
    let pageData = DeepCopy($.mobile.pageData);
    let uri = '../queries/' + pageData.query + '/answers/' + pageData.answer + '/retrieve';

    $.ajax({
      url: '../system',
      dataType: 'json',
      async: false,
      cache: false,
      success: function(system) {
        $('#retrieve-target').val(system['DicomAet']);

        $('#retrieve-form').submit(function(event) {
          event.preventDefault();

          let aet = $('#retrieve-target').val();
          if (aet.length == 0) {
            aet = system['DicomAet'];
          }

          $.ajax({
            url: uri,
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
            success: function() {
              if (pageData.study) {
                // Go back to the list of series
                ChangePage('query-retrieve-3', {
                  'server' : pageData.server,
                  'uuid' : pageData.study
                });
              } else {
                // Go back to the list of studies
                ChangePage('query-retrieve-2', {
                  'server' : pageData.server,
                  'uuid' : pageData.query
                });
              }
            },
            error: function() {
              alert('Error during retrieve');
            }
          });
        });
      }
    });
  }
});
