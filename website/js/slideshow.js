// We don't know the actual rendered size of the main photo until after it's loaded, so that's
// when we can calculate how much margin to apply to the main photo.
function mainphoto_onload(img) {
  var top_margin = ($("#photo-background").height() - img.height) / 2;
  $("#photo-background .mainphoto").css('margin-top', top_margin);
}

(function() {
  var current_racer_id = 0;
  // This cache breaker value is for the title slide image.  We want that image
  // to be cached for the duration of the slideshow.  Reloading the slideshow
  // will pick up any changes to the title slide image.
  var cachebreaker = Date.now();
  var kiosk_parameters = {};
  KioskPoller.param_callback = function(parameters) {
    kiosk_parameters = parameters;
  };

  function refresh_page(racer) {
    var div = $("#photo-background");
    div.empty();
    if (racer) {
      div.append('<img class="mainphoto" onload="mainphoto_onload(this)" src="' +
                 racer.getAttribute('main_photo') + '"/>');
      div.append('<p class="subtitle">' + 
                 '<span class="carno">' + racer.getAttribute('carnumber') + '</span>: ' +
                 racer.getAttribute('name') +
                 (racer.getAttribute('carname') ?
                  '<br/><i>' + racer.getAttribute('carname') + '</i>' : '') +
                 '</p>');
      if (racer.hasAttribute('inset_photo')) {
        div.append('<img class="inset_photo" src="' +
                   racer.getAttribute('inset_photo') + '"/>');
      }
      // Preload the next image for better display
      if (racer.hasAttribute('next_photo')) {
        div.append('<img style="display: none;" src="' +
                   racer.getAttribute('next_photo') + '"/>');
      }
    } else {
      var img = "photo.php/info/slideshow-title/" + cachebreaker + "/img/derby_car.png";
      div.append('<img class="mainphoto" onload="mainphoto_onload(this)" src="' + img + '"/>');
      if (kiosk_parameters.title) {
        $('<p class="maintitle"></p>').text(kiosk_parameters.title).appendTo(div);
      }
    }
  }

  function photo_poll() {
    var classids = kiosk_parameters.classids;
    $.ajax('action.php',
           {type: 'GET',
            data: {query: 'photo.next',
                   racerid: current_racer_id,
                   classids: classids && classids.length > 0 ? classids.join(',') : ''},
            success: function(data) {
              var racers = data.getElementsByTagName("racer");
              if (racers.length > 0) {
                current_racer_id = racers[0].getAttribute('racerid');
                refresh_page(racers[0]);
              } else {
                current_racer_id = 0;
                refresh_page(null);
              }
            }
           }
          );
  }

  $(document).ready(function() {
    photo_poll();
    setInterval(photo_poll, 10000);
  });
}());
