// This file performs the file reading step
// Actual preprocessing is done in dbus_timeline_vis.js

function MyFloatMillisToBigIntUsec(x) {
  x = ('' + x).split('.');
  ret = BigInt(x[0]) * BigInt(1000);
  return ret;
}

// When the Open File dialog is completed, the name of the opened file will be passed
// to this routine. Then the program will do the following:
// 1. Launch "linecount.py" to get the total packet count in the PCAP file for
//    progress
// 2. Launch "dbus-pcap" to get the timestamps of each DBus message
// 3. Launch "dbus-pcap" to get the JSON representation of each DBus message
//
function OpenDBusPcapFile(file_name) {
  // First try to parse using dbus-pcap
  
  ShowBlocker('Determining the number of packets in the pcap file ...');
  const num_lines_py =
      spawn('python3', ['linecount.py', file_name]);
  let stdout_num_lines = '';
  num_lines_py.stdout.on('data', (data) => {
    stdout_num_lines += data;
  });

  num_lines_py.on('close', (code) => {
    let num_packets = parseInt(stdout_num_lines.trim());
    ShowBlocker('Running dbus-pcap (Pass 1/2, packet timestamps) ...');
    const dbus_pcap1 =
        //spawn('python3', ['dbus-pcap', file_name, '--json', '--progress']);
        spawn('python3', ['dbus-pcap', file_name]);
    let stdout1 = '';
    let timestamps = [];
    let count1 = 0; // In the first phase, consecutive newlines indicate a new entry
    //const r = new RegExp('([0-9]+/[0-9]+) [0-9]+\.[0-9]+:.*');
    const r = new RegExp('([0-9]+\.[0-9]+):.*');

    let is_last_newline = false;  // Whether the last scanned character is a newline
    let last_update_millis = 0;
    dbus_pcap1.stdout.on('data', (data) => {
      const s = data.toString();
      stdout1 += s;
      for (let i=0; i<s.length; i++) {
        const ch = s[i];
        let is_new_line = false;
        if (ch == '\n' || ch == '\r') {
          is_new_line = true;
        }
        if (!is_last_newline && is_new_line) {
          count1 ++;
        }
        is_last_newline = is_new_line;
      }
      const millis = Date.now();
      if (millis - last_update_millis > 100) { // Refresh at most 10 times per second
        let pct = parseInt(count1 * 100 / num_packets);
        ShowBlocker('Running dbus-pcap (Pass 1/2, packet timestamps): ' + count1 + '/' + num_packets + ' (' + pct + '%)');
        last_update_millis = millis;
      }
    });

    dbus_pcap1.on('close', (code) => {
      ShowBlocker('Running dbus-pcap (Pass 2/2, packet contents) ...');
      let stdout2 = '';
      let count2 = 0;
      is_last_newline = false;
      const dbus_pcap2 =
          spawn('python3', ['dbus-pcap', file_name, '--json']);
      dbus_pcap2.stdout.on('data', (data) => {
        const s = data.toString();
        stdout2 += s;
        for (let i=0; i<s.length; i++) {
          const ch = s[i];
          let is_new_line = false;
          if (ch == '\n' || ch == '\r') {
            is_new_line = true;
          }
          if (!is_last_newline && is_new_line) {
            count2 ++;
          }
          is_last_newline = is_new_line;
        }
        const millis = Date.now();
        if (millis - last_update_millis > 100) { // Refresh at most 10 times per second
          let pct = parseInt(count2 * 100 / num_packets);
          ShowBlocker('Running dbus-pcap (Pass 2/2, packet contents): ' + count2 + '/' + num_packets + ' (' + pct + '%)');
          last_update_millis = millis;
        }
      });

      dbus_pcap2.on('close', (code) => {
        { ShowBlocker('Processing dbus-pcap output ... '); }

        let packets = [];
        // Parse timestamps
        let lines = stdout1.split('\n');
        for (let i=0; i<lines.length; i++) {
          let l = lines[i].trim();
          if (l.length > 0) {
            // Timestamp
            l = l.substr(0, l.indexOf(':'));
            const ts_usec = parseFloat(l) * 1000.0;
            if (!isNaN(ts_usec)) {
              timestamps.push(ts_usec);
            } else {
              console.log('not a number: ' + l);
            }
          }
        }

        // JSON
        lines = stdout2.split('\n');
        for (let i=0; i<lines.length; i++) {
          let l = lines[i].trim();
          let parsed = undefined;
          if (l.length > 0) {
            try {
              parsed = JSON.parse(l);
            } catch (e) {
              console.log(e);
            }

            if (parsed == undefined) {
              try {
                l = l.replace("NaN", "null");
                parsed = JSON.parse(l);
              } catch (e) {
                console.log(e);
              }
            }

            if (parsed != undefined) {
              packets.push(parsed);
            }
          }
        }

        Timestamps_DBus = timestamps;

        Data_DBus = packets.slice();
        OnGroupByConditionChanged_DBus();
        const v = dbus_timeline_view;

        // Will return 2 packages
        // 1) sensor PropertyChagned signal emissions
        // 2) everything else
        let preproc = Preprocess_DBusPcap(packets, timestamps);

        let grouped = Group_DBus(preproc, v.GroupBy);
        GenerateTimeLine_DBus(grouped);

        dbus_timeline_view.IsCanvasDirty = true;
        if (dbus_timeline_view.IsEmpty() == false ||
            ipmi_timeline_view.IsEmpty() == false) {
          dbus_timeline_view.CurrentFileName = file_name;
          ipmi_timeline_view.CurrentFileName = file_name;
          HideWelcomeScreen();
          ShowDBusTimeline();
          ShowIPMITimeline();
          ShowNavigation();
          UpdateFileNamesString();
        }
        HideBlocker();

        g_btn_zoom_reset.click(); // Zoom to capture time range
      });
    });
  })
}

// Input: data and timestamps obtained from 
// Output: Two arrays
//   The first is sensor PropertyChanged emissions only
//   The second is all other DBus message types
//
// This function also determines the starting timestamp of the capture
//
function Preprocess_DBusPcap(data, timestamps) {
  // Also clear IPMI entries
  g_ipmi_parsed_entries = [];

  let ret = [];

  let in_flight = {};
  let in_flight_ipmi = {};

  for (let i = 0; i < data.length; i++) {
    const packet = data[i];

    // Fields we are interested in
    const fixed_header = packet[0];  // is an [Array(5), Array(6)]

    if (fixed_header == undefined) {  // for hacked dbus-pcap
      console.log(packet);
      continue;
    }

    const payload = packet[1];
    const ty = fixed_header[0][1];
    let timestamp = timestamps[i];
    let timestamp_end = undefined;
    const IDX_TIMESTAMP_END = 8;
    const IDX_MC_OUTCOME = 9;  // Outcome of method call

    let serial, path, member, iface, destination, sender, signature = '';
    // Same as the format of the Dummy data set

    switch (ty) {
      case 4: {  // signal
        serial = fixed_header[0][5];
        path = fixed_header[1][0][1];
        iface = fixed_header[1][1][1];
        member = fixed_header[1][2][1];
        // signature = fixed_header[1][3][1];
        // fixed_header[1] can have variable length.
        // For example: signal from org.freedesktop.PolicyKit1.Authority can
        // have only 4 elements, while most others are 5
        const idx = fixed_header[1].length - 1;
        sender = fixed_header[1][idx][1];

        // Ugly fix for:
        if (sender == "s" || sender == "sss") {
          sender = packet[1][0];
          if (fixed_header[1].length == 6) {
            // Example: fixed_header is
            // 0: (2) [7, "org.freedesktop.DBus"]
            // 1: (2) [6, ":1.1440274"]
            // 2: (2) [1, "/org/freedesktop/DBus"]
            // 3: (2) [2, "org.freedesktop.DBus"]
            // 4: (2) [3, "NameLost"]
            // 5: (2) [8, "s"]
            path = fixed_header[1][2][1];
            iface = fixed_header[1][3][1];
            member = fixed_header[1][4][1];
          } else if (fixed_header[1].length == 5) {
            // Example:  fixed_header is
            // 0: (2) [7, "org.freedesktop.DBus"]
            // 1: (2) [1, "/org/freedesktop/DBus"]
            // 2: (2) [2, "org.freedesktop.DBus"]
            // 3: (2) [3, "NameOwnerChanged"]
            // 4: (2) [8, "sss"]
            path = fixed_header[1][1][1];
            iface = fixed_header[1][2][1];
            member = fixed_header[1][3][1];
          }
        }


        destination = '<none>';
        timestamp_end = timestamp;
        let entry = [
          'sig', timestamp, serial, sender, destination, path, iface, member,
          timestamp_end, payload
        ];

        // Legacy IPMI interface uses signal for IPMI request
        if (iface == 'org.openbmc.HostIpmi' && member == 'ReceivedMessage') {
          console.log('Legacy IPMI interface, request');
        }

        ret.push(entry);
        break;
      }
      case 1: {  // method call
        serial = fixed_header[0][5];
        path = fixed_header[1][0][1];
        member = fixed_header[1][1][1];
        iface = fixed_header[1][2][1];
        destination = fixed_header[1][3][1];
        if (fixed_header[1].length > 5) {
          sender = fixed_header[1][5][1];
          signature = fixed_header[1][4][1];
        } else {
          sender = fixed_header[1][4][1];
        }
        let entry = [
          'mc', timestamp, serial, sender, destination, path, iface, member,
          timestamp_end, payload, ''
        ];

        // Legacy IPMI interface uses method call for IPMI response
        if (iface == 'org.openbmc.HostIpmi' && member == 'sendMessge') {
          console.log('Legacy IPMI interface, response')
        } else if (
            iface == 'xyz.openbmc_project.Ipmi.Server' && member == 'execute') {
          let ipmi_entry = {
            netfn: payload[0],
            lun: payload[1],
            cmd: payload[2],
            request: payload[3],
            start_usec: MyFloatMillisToBigIntUsec(timestamp),
            end_usec: 0,
            response: []
          };
          in_flight_ipmi[serial] = (ipmi_entry);
        }


        ret.push(entry);
        in_flight[serial] = (entry);
        break;
      }
      case 2: {  // method reply
        let reply_serial = fixed_header[1][0][1];
        if (reply_serial in in_flight) {
          let x = in_flight[reply_serial];
          delete in_flight[reply_serial];
          x[IDX_TIMESTAMP_END] = timestamp;
          x[IDX_MC_OUTCOME] = 'ok';
        }

        if (reply_serial in in_flight_ipmi) {
          let x = in_flight_ipmi[reply_serial];
          delete in_flight_ipmi[reply_serial];
          if (payload[0] != undefined && payload[0][4] != undefined) {
            x.response = payload[0][4];
          }
          x.end_usec = MyFloatMillisToBigIntUsec(timestamp);
          g_ipmi_parsed_entries.push(x);
        }
        break;
      }
      case 3: {  // error reply
        let reply_serial = fixed_header[1][0][1];
        if (reply_serial in in_flight) {
          let x = in_flight[reply_serial];
          delete in_flight[reply_serial];
          x[IDX_TIMESTAMP_END] = timestamp;
          x[IDX_MC_OUTCOME] = 'error';
        }
      }
    }
  }

  if (g_ipmi_parsed_entries.length > 0) UpdateLayout();
  return ret;
}
