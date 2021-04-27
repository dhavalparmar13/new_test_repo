<?php
/**
 * @package FS_CURL
 * @subpackage FS_CURL_Configuration
 * conference.conf.php
 */
if (basename($_SERVER['PHP_SELF']) == basename(__FILE__)) {
    header('Location: index.php');
}

/**
 * @package  FS_CURL
 * @subpackage FS_CURL_Configuration
 * @license
 * @author Raymond Chandler (intralanman) <intralanman@gmail.com>
 * @version 0.1
 * Write XML for conference.conf
*/
class conference_conf extends fs_configuration {
    /**
     * Initializer method
     * This method calls fs_configuration to initialize all of the
     * objects and variables that we want to inherit
     * @return void
    */
/*    public function conference_conf() {
        $this -> fs_configuration();
    }
 */
    /**
     * Main sub-routine
     * This method will call all of the other methods necessary
     * to write out the XML for the conference.conf
     * @return void
    */
    public function main() {
	$this -> xmlw -> startElement('configuration');
	$this -> xmlw -> writeAttribute('name', 'conference.conf');
	$this -> xmlw -> writeAttribute('description', 'Audio Conference');
	$this -> xmlw -> startElement('advertise');
	$this -> xmlw -> writeAttribute('name', '3001@$${domain}');
	$this -> xmlw -> writeAttribute('status', 'FreeSWITCH');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('caller-controls');
	$this -> xmlw -> startElement('group');
	$this -> xmlw -> writeAttribute('name', 'default');
	$this -> xmlw -> startElement('control');
	$this -> xmlw -> writeAttribute('action', 'mute');
	$this -> xmlw -> writeAttribute('digits', '0');
	$this -> xmlw -> endElement();
	$this -> xmlw -> endElement();
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('profiles');
	$this -> xmlw -> startElement('profile');
	$this -> xmlw -> writeAttribute('name', 'default');
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'domain');
	$this -> xmlw -> writeAttribute('value', '$${domain}');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'rate');
	$this -> xmlw -> writeAttribute('value', '48000');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'interval');
	$this -> xmlw -> writeAttribute('value', '20');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'energy-level');
	$this -> xmlw -> writeAttribute('value', '100');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'muted-sound');
	$this -> xmlw -> writeAttribute('value', 'conference/conf-muted.wav');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'unmuted-sound');
	$this -> xmlw -> writeAttribute('value', 'conference/conf-unmuted.wav');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'alone-sound');
	$this -> xmlw -> writeAttribute('value', 'conference/conf-alone.wav');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'moh-sound');
	$this -> xmlw -> writeAttribute('value', 'conference/conf-alone.wav');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'enter-sound');
	$this -> xmlw -> writeAttribute('value', 'tone_stream://%(200,0,500,600,700)');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'exit-sound');
	$this -> xmlw -> writeAttribute('value', 'tone_stream://%(500,0,300,200,100,50,25)');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'kicked-sound');
	$this -> xmlw -> writeAttribute('value', 'conference/conf-kicked.wav');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'locked-sound');
	$this -> xmlw -> writeAttribute('value', 'conference/conf-locked.wav');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'is-locked-sound');
	$this -> xmlw -> writeAttribute('value', 'conference/conf-is-locked.wav');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'is-unlocked-sound');
	$this -> xmlw -> writeAttribute('value', 'conference/conf-is-unlocked.wav');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'pin-sound');
	$this -> xmlw -> writeAttribute('value', 'conference/conf-pin.wav');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'bad-pin-sound');
	$this -> xmlw -> writeAttribute('value', 'conference/conf-bad-pin.wav');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'caller-id-name');
	$this -> xmlw -> writeAttribute('value', '$${outbound_caller_name}');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'caller-id-number');
	$this -> xmlw -> writeAttribute('value', '$${outbound_caller_id}');
	$this -> xmlw -> endElement();
	$this -> xmlw -> startElement('param');
	$this -> xmlw -> writeAttribute('name', 'comfort-noise');
	$this -> xmlw -> writeAttribute('value', 'true');
	$this -> xmlw -> endElement();
	$this -> xmlw -> endElement();
	$this -> xmlw -> endElement();

/*	$this -> write_advertises();
	if($this -> request['Controls']) {$this -> write_controls();}
	if($this -> request['profile_name']) {$this -> write_profiles();}
 */
	$this -> xmlw -> endElement();
    }

    /**
     * Write XML for <advertises>
     * This method will write the elements and attributes for the
     * conferences that are to be advertised
     * @return void
    */
    private function write_advertises() {
        $query = "SELECT * FROM conference_advertise ORDER BY room";
        $advertises = $this -> db -> queryAll($query);
        if (FS_PDO::isError($advertises)) {
            $this -> comment($query);
            $this -> comment($this -> db -> getMessage());
            return ;
        }
        $advertise_count = count($advertises);
        if ($advertise_count < 1) {
            return ;
        }
        $this -> xmlw -> startElement('advertise');
        for ($i=0; $i<$advertise_count; $i++) {
            //$this -> comment_array($advertises[$i]);
            $this -> xmlw -> startElement('room');
            $this -> xmlw -> writeAttribute('name', $advertises[$i]['room']);
            $this -> xmlw -> writeAttribute('status', $advertises[$i]['status']);
            $this -> xmlw -> endElement();
        }
        $this -> xmlw -> endElement();
    }


    /**
     * Pull conference profiles from the database
     * This method will pull the conference profiles
     * from the database using the PDO extension
     * @return array
    */
    private function get_profiles_array() {
	
	$prof = $this -> request['profile_name'];
        $query = "SELECT * FROM conference_profiles where profile_name='$prof'";
        $res = $this -> db -> query($query);
        if (FS_PDO::isError($res)) {
            $this -> comment($query);
            $this -> comment($this -> db -> getMessage());
            return array();
        }
        while ($row = $res -> fetch(PDO::FETCH_ASSOC)) {
            $pn = $row['profile_name'];
            $profiles[$pn][] = $row;
        }
        return $profiles;
    }

    /**
     * Write XML for <profiles>
     * This method will write the XML of the array
     * from get_profiles_array
     * @return void
    */
    private function write_profiles() {
        $profiles_array = $this -> get_profiles_array();
        $profiles_count = count($profiles_array);
        if ($profiles_count < 1) {
            return ;
        }
        $this -> xmlw -> startElement('profiles');

        foreach ($profiles_array as $profile_name => $profile_data) {
            $this -> xmlw -> startElement('profile');
            $this -> xmlw -> writeAttribute('name', $profile_name);
            foreach ($profile_data as $params) {
                //$this -> comment_array($profiles_array[$i]);
                $this -> xmlw -> startElement('param');
                $this -> xmlw -> writeAttribute('name', $params['param_name']);
                $this -> xmlw -> writeAttribute('value', $params['param_value']);
                $this -> xmlw -> endElement();//</param>

            }
            $this -> xmlw -> endElement();
        }
        $this -> xmlw -> endElement();
    }

    /**
     * Pull caller-controls from database via PDO
     * This method will pull the conference caller-controls from
     * the database using the PDO extension
     * @return array
    */
    private function get_controls_array() {
        $cont = $this -> request['Controls'];
		$query = sprintf(
		  "SELECT * FROM conference_controls where conf_group = '$cont'"
        );
        $res = $this -> db -> query($query);
        if (FS_PDO::isError($res)) {
            $this -> comment($query);
            $this -> comment($this -> db -> getMessage());
            return array();
        }
        while ($row = $res -> fetch(PDO::FETCH_ASSOC)) {
            $cg = $row['conf_group'];
            $profiles[$cg][] = $row;
        }
        return $profiles;
    }

    /**
     * Write XML for <caller-controls>
     * This method will write the XML of the array
     * from get_controls_array
     * @return void
    */
    private function write_controls() {
        $controls_array = $this -> get_controls_array();
        $controls_count = count($controls_array);
        if (FS_PDO::isError($controls_array)) {
            $this -> comment($query);
            $this -> comment($this -> db -> getMessage());
            return ;
        }
        if ($controls_count < 1) {
            return ;
        }
        $this -> xmlw -> startElement('caller-controls');

        foreach ($controls_array as $group_name => $control_data) {
            $this -> xmlw -> startElement('group');
            $this -> xmlw -> writeAttribute('name', $group_name);
            foreach ($control_data as $controls) {

                //$this -> comment_array($controls_array[$i]);
                $this -> xmlw -> startElement('control');
                $this -> xmlw -> writeAttribute('action', $controls['action']);
                $this -> xmlw -> writeAttribute('digits', $controls['digits']);
		if($controls['data']) {$this -> xmlw -> writeAttribute('data', $controls['data']);}
                $this -> xmlw -> endElement();

            }
            $this -> xmlw -> endElement();
        }
        $this -> xmlw -> endElement();
    }
}
?>
