<?php
	/**
	 * @package    FS_CURL
	 * @subpackage FS_CURL_Directory
	 *             fs_directory.php
	 */
	if ( basename( $_SERVER['PHP_SELF'] ) == basename( __FILE__ ) ) {
		header( 'Location: index.php' );
	}

	/**
	 * @package    FS_CURL
	 * @subpackage FS_CURL_Directory
	 * @author     Raymond Chandler (intralanman) <intralanman@gmail.com>
	 * @license    BSD
	 * @version    0.1
	 *             Class for XML directory
	 */
	class fs_directory extends fs_curl {

		private $user;
		private $key;
		private $domain;
		private $userid;
		private $users_vars;
		private $users_params;
		private $users_gateways;
		private $users_gateway_params;

		public function fs_directory() {
        		self::__construct();
    		}
	
		public function __construct() {
			$this->fs_curl();
			if ( array_key_exists( 'user', $this->request ) ) {
				$this->user = $this->request['user'];
			}
			if( array_key_exists( 'domain', $this->request ) ){
				$this->domain = $this->request['domain'];
			}
			//			$this->comment( "User is " . $this->user);
		}

		public function main() {
			//			$this->comment( $this->request );
			$this->xmlw->startElement( 'section' );
			$this->xmlw->writeAttribute( 'name', 'directory' );
			$this->xmlw->writeAttribute( 'description', 'FreeSWITCH Directory' );

			$this->xmlw->startElement( 'domain' );
			$this->xmlw->writeAttribute( 'name', $this->domain );
			$this->xmlw->startElement( 'user' );
			$this->xmlw->writeAttribute( 'id', $this->user );
			$this->get_user_data();			
			$this->print_default_vars();	
			$this->xmlw->endElement(); // </user>
			$this->xmlw->endElement(); // </section>
			$this->output_xml();
		}

		private function get_user_data(){
			//$where=sprintf("where ani_number='%s' limit 1",$this->user);
			//$query=sprintf("SELECT ani_password from is_ani_numbers %s",$where);
			//$where=sprintf("where sip_username='%s' limit 1",$this->user);
			//$query=sprintf("SELECT sip_password from is_sip_account %s",$where);
			$where=sprintf("where username='%s' limit 1",$this->user);
			$query=sprintf("SELECT password from temp %s",$where);
			$res   = $this->db->queryAll( $query );

			if ( FS_PDO::isError( $res ) ) {
				$this->comment( $query );
				$error_msg = sprintf( "Error while selecting params - %s", $this->db->getMessage() );
				trigger_error( $error_msg );
				$this->file_not_found();
			}

			//				$ram_count = count( $res );
			//				$this->comment( "result is " . $res[0][sip_password].$ram_count );
			//$this->key='ani_password';
			//$this->key='sip_password';
			$this->key='password';
			if(!$res[0][$this->key]){
				$this->file_not_found();
			}
			$this->xmlw->startElement( 'params' );
			$this->xmlw->startElement( 'param' );
			$this->xmlw->writeAttribute( 'name','password' );
			$this->xmlw->writeAttribute( 'value', $res[0][$this->key] );
			$this->xmlw->endElement(); // </param>
/*
			$this->xmlw->startElement( 'param' );
			$this->xmlw->writeAttribute( 'name', 'verto-context' );
			$this->xmlw->writeAttribute( 'value','inmate_call' );
			$this->xmlw->endElement();

 */
			$this->xmlw->startElement( 'param' );
			$this->xmlw->writeAttribute( 'name', 'dialplan' );
			$this->xmlw->writeAttribute( 'value', 'XML' );
			$this->xmlw->endElement();
/*
			$this->xmlw->startElement( 'param' );
			$this->xmlw->writeAttribute( 'name','jsonrpc-allowed-methods' );
			$this->xmlw->writeAttribute( 'value','verto' );
			$this->xmlw->endElement();

			$this->xmlw->startElement( 'param' );
			$this->xmlw->writeAttribute( 'name','jsonrpc-allowed-event-channels' );
			$this->xmlw->writeAttribute( 'value','demo,conference,presence' );
			$this->xmlw->endElement();
*/
			$this->xmlw->endElement(); // </params>
		}

		private function print_default_vars(){
			$this->xmlw->startElement( 'variables' );

			$this->xmlw->startElement( 'variable' );
			$this->xmlw->writeAttribute( 'name', 'tollallow' );
			$this->xmlw->writeAttribute( 'value', 'domestic,international,local' );
			$this->xmlw->endElement(); // </variable>

			$this->xmlw->startElement( 'variable' );
			$this->xmlw->writeAttribute( 'name', 'user_context' );
			$this->xmlw->writeAttribute( 'value', 'default' );
			$this->xmlw->endElement(); // </variable>

			$this->xmlw->endElement(); // </variables>
		}

	}		
