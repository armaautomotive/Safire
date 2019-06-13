<?php
/**
* relayqueue.php
*
* Description: Is this user readable only?
*
*/
include_once("db.php");

date_default_timezone_set('America/Los_Angeles');

$sender_key = isset( $_POST['sender_key'] ) ? $_POST['sender_key'] : $_GET['sender_key'];
$receiver_key = isset( $_POST['receiver_key'] ) ? $_POST['receiver_key'] : $_GET['receiver_key'];
$connection_string = isset( $_POST['connection_string'] ) ? $_POST['connection_string'] : $_GET['connection_string'];
$action = isset( $_POST['action'] ) ? $_POST['action'] : $_GET['action'];
$message = isset( $_POST['message'] ) ? $_POST['message'] : $_GET['message'];
$type = isset( $_POST['type'] ) ? $_POST['type'] : $_GET['type'];
$return_request_data = isset( $_POST['return_request_data'] ) ? $_POST['return_request_data'] : $_GET['return_request_data'];

//if( $receiver_key != ''){
        echo "Messages <br>";
        $pdo = new PDO('mysql:dbname='.$db_name.';host='.$db_host, $db_user, $db_pass);
        $sql = "select * from messages ".
	// where receiver_key = :receiver_key" . 
	"  order by time desc limit 0, 1000;";
        $stmt = $pdo->prepare($sql);
        $params = array();
        //$params["receiver_key"] = $receiver_key;
        //$params["type"] = $type;
        $r = $stmt->execute($params);
        $results = $stmt->fetchAll(PDO::FETCH_ASSOC);
        if( sizeof($results) > 0 ){
                $sent_ids = array();
                $data = array();
                $message_result = "";
                foreach($results as $result){
                        $data_record = array();
                        $data_record["receiver_key"] = $result["receiver_key"];
                        $data_record["sender_key"] = $result["sender_key"];
                        $data_record["request"] = $result["request"];
                        $data_record["message"] = $result["message"];
                        $data_record["id"] = $result["id"];
                        array_push($data, $data_record);
                        array_push($sent_ids, $result["id"]);

			echo "<br>";
			echo "Sender: ". $result["sender_key"]." -> ".$result["receiver_key"] . " <br>";
			echo "Time: " . $result["time"] . "<br>";
			$message = $result["message"];
			
			for( $i = 1; $i < strlen($message); $i++ ){
				if( substr($message, $i, 1) == '{' ){
					$message = substr( $message, 0, $i + 1 ) . "<br>" . substr( $message, $i + 2 ) ;
				}
			} 

			echo "<table cellpadding='2' border='1' ><tr><td><font size='2'>" . $message . "</font></td></tr></table>";

                        if($return_request_data == "true"){
                            $message_result .= json_encode($data_record);
                        } else {
                            $message_result .= $result["message"] . "\n";
                        }
                }
                //echo json_encode($data);
                echo $message_result;


	}
//}

// Delete messages that are 24 hours old.

?>

