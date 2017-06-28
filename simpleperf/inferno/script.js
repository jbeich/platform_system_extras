<script type="text/ecmascript">

function init() {
    var x = document.getElementsByTagName("svg")
    for (i = 0; i < x.length; i=i+1) {
        adjust_text_size(x[i])
    }
}

function adjust_node_text_size(x) {

      title = x.getElementsByTagName("title")[0];
      text = x.getElementsByTagName("text")[0];
      rect = x.getElementsByTagName("rect")[0];

      width = parseFloat(rect.attributes["width"].value);

      // Don't even bother trying to find a best fit. The area is too small.
      if (width < 25) {
          text.textContent = "";
          return;
      }
      // Remove dso and #samples which are here only for mouseover purposes.
      methodName = title.textContent.substring(0, title.textContent.indexOf("|"));

      var numCharacters;
      for (numCharacters=methodName.length; numCharacters>4; numCharacters--) {
         // Avoid reflow by using hard-coded estimate instead of text.getSubStringLength(0, numCharacters)
         // if (text.getSubStringLength(0, numCharacters) <= width) {
			if (numCharacters * 7.5 <= width) {
				break ;
			}
	  }

	  if (numCharacters == methodName.length) {
	  text.textContent = methodName;
	     return
	  }

      text.textContent = methodName.substring(0, numCharacters-2) + "..";
 }

function adjust_text_size(svgElement) {

    var x = svgElement.getElementsByTagName("g");
    var i;
    for (i=0 ; i < x.length ; i=i+1) {
        adjust_node_text_size(x[i])
    }
}

function zoom(e) {
    var clicked_rect = e.getElementsByTagName("rect")[0];
    var clicked_origin_x = clicked_rect.attributes["ox"].value;
    var clicked_origin_y = clicked_rect.attributes["oy"].value;
    var clicked_origin_width = clicked_rect.attributes["owidth"].value;


    var svgBox = e.ownerSVGElement.getBoundingClientRect();
    var svgBoxHeight = svgBox.height
    var svgBoxWidth = svgBox.width
    var scaleFactor = svgBoxWidth/clicked_origin_width;

    var callsites = e.ownerSVGElement.getElementsByTagName("g");
    var i;
    for (i = 0; i < callsites.length; i=i+1) {
      text = callsites[i].getElementsByTagName("text")[0];
      rect = callsites[i].getElementsByTagName("rect")[0];

      rect_o_x = rect.attributes["ox"].value
      rect_o_y = parseFloat(rect.attributes["oy"].value)

      // Avoid multiple forced reflow by hiding nodes.
      if (rect_o_y > clicked_origin_y) {
         rect.style.display = "none"
         text.style.display = "none"
         continue;
      } else {
         rect.style.display = "block"
         text.style.display = "block"
      }

      rect.attributes["x"].value = newrec_x = (rect_o_x - clicked_origin_x) * scaleFactor ;
      rect.attributes["y"].value = newrec_y = rect_o_y + (svgBoxHeight - clicked_origin_y - 17 -2);

      text.attributes["y"].value = newrec_y + 12;
      text.attributes["x"].value = newrec_x + 4;

      rect.attributes["width"].value = rect.attributes["owidth"].value * scaleFactor;
    }

    adjust_text_size(e.ownerSVGElement);

    e.ownerSVGElement.getElementById("zoom_rect").style.display = "block";
    e.ownerSVGElement.getElementById("zoom_text").style.display = "block";
}

function unzoom(e) {
  var svgOwner = e.ownerSVGElement
  var callsites = e.ownerSVGElement.getElementsByTagName("g");
  zoom(callsites[0])
  svgOwner.getElementById("zoom_rect").style.display = "none";
  svgOwner.getElementById("zoom_text").style.display = "none";
}

function search(e) {
  var term = prompt("Search for:", "");

  var svgOwner = e.ownerSVGElement
  var callsites = e.ownerSVGElement.getElementsByTagName("g");

  if (term == null || term == "") {
    for (i = 0; i < callsites.length; i=i+1) {
      rect = callsites[i].getElementsByTagName("rect")[0];
      rect.attributes["fill"].value = rect.attributes["ofill"].value;
    }
    return;
  }

  for (i = 0; i < callsites.length; i=i+1) {
      title = callsites[i].getElementsByTagName("title")[0];
      rect = callsites[i].getElementsByTagName("rect")[0];
      if (title.textContent.indexOf(term) != -1) {
        rect.attributes["fill"].value = "rgb(230,100,230)";
      } else {
        rect.attributes["fill"].value = rect.attributes["ofill"].value;
      }
  }

}

</script>