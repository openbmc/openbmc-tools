// This function accomplishes drag-and-drop gestures by attaching temporary onmouse{up,move} events to the document
function DragElement(elt) {
  var x1=0, y1=0, x2=0, y2=0;
  elt.onmousedown = DragMouseDown;

  function DragMouseDown(e) {
    e = e || window.event;
    e.preventDefault();
    // get the mouse cursor position at startup:
    x2 = e.clientX;
    y2 = e.clientY;
    document.onmouseup = MouseUp;
    // call a function whenever the cursor moves:
    document.onmousemove = MouseMove;
  }

  function MouseMove(e) {
    e = e || window.event;
    e.preventDefault();
    x1 = x2 - e.clientX; y1 = y2 - e.clientY;
    x2 = e.clientX; y2 = e.clientY;
    elt.style.top = (elt.offsetTop - y1) + "px";
    elt.style.left= (elt.offsetLeft- x1) + "px";
  }

  function MouseUp() {
    document.onmouseup = null;
    document.onmousemove = null;
  }
}

function GetTotalCount(titles_and_intervals) {
  let ret = 0;
  titles_and_intervals.forEach((ti) => {
    ret += ti[1].length;
  });
  return ret;
}

function UpdateHighlightedMessagesInfoPanel() {
  const ti_dbus = dbus_timeline_view.HighlightedMessages();
  const ti_ipmi = ipmi_timeline_view.HighlightedMessages();

  const x = document.getElementById("highlighted_messages_content");
  let h = ""
  const count_dbus = GetTotalCount(ti_dbus);
  if (count_dbus > 0) {
    h += count_dbus + " DBus messages<br/>";
  }
  const count_ipmi = GetTotalCount(ti_ipmi);
  if (count_ipmi > 0) {
    h += count_ipmi + " IPMI messages<br/>";
  }
  x.innerHTML = h;
}