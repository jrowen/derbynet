<ondeck>
<?php
require_once('inc/current-heat.inc');
require_once('inc/autoadvance.inc');
require_once('inc/ordinals.inc');

// This script produces XML in response to a request from a page that wants to 
// update itself as the race progresses.  In particular, it supports ondeck.php (showing 
// race results by heat, as well as the schedule for future heats) and racer-results.php
// (showing race results organized by racer).
//
// The updates that each of these pages has to track comes in different flavors:
//
// (1) The heat that's "current" advances.
// (2) New heat results (race times) have been recorded since the last update.
// (3) New heats have been scheduled for a racing round that we already knew about.
//     (That is, there was an entry in the Rounds table, even if the heats hadn't 
//     yet been scheduled.)
// (4) New racing rounds have been scheduled (new entries in Rounds table).
//
// Deletions are also possible, but relatively rare.  We don't worry about these, and
// instead rely on updates being triggered by new, replacement data.
//
// This script returns an XML document that looks like:
//
// <ondeck>
//   <current-heat classid="{classid}"
//                 roundid="{roundid}"
//                 round="{round}"
//                 tbodyid="{tbodyid}"
//                 heat="{heat number}"
//     >{class name}</class>
//   <lanes n="{nlanes}"/>
//   <options update-period="{period in ms}" use-master-sched="(0 or 1)"/>
//
//   <update resultid="{resultid}" result="{finishtime, as n.nnn}"/>
//   <update resultid="{resultid}" result="{finishtime, as n.nnn}"/>
//   ...
//
//   <has_new_schedule tbodyid="{tbodyid}"/>
//   ...
//
//   <high_water roundid= round= tbodyid= resultid= completed= />
// </ondeck>
//
// The class, round, and heat elements identify the "current" heat.
//
// The update elements provide heat times for heats that completed after the page's last update.
// The client-side page, via update.js, passes a date/time parameter named 'since' to identify
// the high water mark of Completed values that it's already displaying.
//
// The most_recent element tells the page what its new high water mark for Completed values is.
//
// The final_round element gives a high water mark for RoundIDs.  When this value changes, relative
// to what the page already knows about, it gives up on incrementality and just reloads the whole page.

// date/time string of last update time (high-water completed)
// Format: '2013-11-14 14:57:14'
$since = @$_GET['since'];
$hwresultid = @$_GET['high_water_resultid'];
if (!$hwresultid) {
	$hwresultid = 0;
}

// If true, the client is using master scheduling and therefore merging all the
// rounds with the same round number.  We identify everything by round number in
// that case, instead of roundid.
$merge_rounds = @$_GET['merge_rounds'];

$use_master_sched = use_master_sched();
$now_running = get_running_round();
emit_current_heat($now_running, $use_master_sched);
?>

<?php
$next = get_next_heat($now_running, 'next');
if ($next !== false) {
  echo '  <next-heat roundid="'.$next['roundid'].'" heat="'.$next['heat'].'"/>'."\n";
} else { echo "<no-next-heat/>\n"; }
?>
  <lanes n="<?php echo get_lane_count_from_results(); ?>"/>
  <options update-period="<?php echo update_period(); ?>"
           use-master-sched="<?php echo $use_master_sched; ?>"/>
<?php
  $sql = 'SELECT'
    .' lane, resultid, finishtime, finishplace, completed'
    .' FROM Rounds'
    .' INNER JOIN RaceChart'
    .' ON RaceChart.roundid = Rounds.roundid'
    .' WHERE (finishtime IS NOT NULL OR finishplace IS NOT NULL)'
    .($since
	    ? ' AND completed > '
	      .($dbtype == 'access'
			  ? 'CDate(:since)' 
            : ($dbtype == 'sqlite'
               ? 'datetime(:since)'
               : 'TIMESTAMP(:since)'))
	    : "") 
    .' ORDER BY completed, resultid, lane';
  $stmt = $db->prepare($sql);
  $stmt->execute($since ? array(':since' => $since) : array());

  if ($stmt === FALSE) {
	$info = $db->errorInfo();
    echo '<error msg="'.$info[2].'" query="'.$sql.'"/>'."\n";
  }

  // <update> elements for heats recorded since the ['since']
  $max_time = $since;
  $use_points = read_raceinfo_boolean('use-points');
  foreach ($stmt as $rs) {
    if ($use_points) {
      $val = ordinal($rs['finishplace']);
    } else {
      // TODO: Adjustable number of decimals?
      $val = number_format($rs['finishtime'], 3);
    }
    echo "<update resultid='".$rs['resultid']."'"
	  ." result='".$val."'/>\n";
    if ($rs['completed'] > $max_time)
      $max_time = $rs['completed'];
  }

  // <has_new_schedule> elements for rounds affected by newly-entered racechart rows

$sql = 'SELECT DISTINCT Rounds.roundid, round'
    .' FROM RaceChart'
    .' INNER JOIN Rounds'
    .' ON RaceChart.roundid = Rounds.roundid'
    .' WHERE resultid > '.$hwresultid
    .' ORDER BY Rounds.roundid';
  $stmt = $db->query($sql);
  if ($stmt === FALSE) {
	$info = $db->errorInfo();
    echo '<error msg="'.$info[2].'" query="'.$sql.'"/>'."\n";
  }

  foreach ($stmt as $rs) {
    echo '<has_new_schedule'
    .' tbodyid="'.$rs[$merge_rounds ? 'round' : 'roundid'].'"'
    .'/>'."\n";
  }

  $high_water_rounds = high_water_rounds();
  ?>
  <high_water roundid="<?php echo $high_water_rounds['roundid']; ?>"
              round="<?php echo $high_water_rounds['round']; ?>"
              tbodyid="<?php echo $high_water_rounds[$merge_rounds ? 'round' : 'roundid']; ?>"
              resultid="<?php echo high_water_resultid(); ?>"
              completed="<?php echo $max_time; ?>" />

</ondeck>
