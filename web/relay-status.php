<?php
/**
*
*
*/
include_once("db.php");
date_default_timezone_set('America/Los_Angeles');
echo "Relay Status";
// receiver_key = :receiver_key && type = :type
$pdo = new PDO('mysql:dbname='.$db_name.';host='.$db_host, $db_user, $db_pass);
$sql = "select * from messages order by time desc limit 0, 100;";
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

			$msg = $result["message"];
 

			echo " <table><tr><td> type ". 
			 	$result["type"]  .	
				" </td></tr><tr><td> sender: ".
				
			 	$result["sender_key"]  .        
                                " </td></tr><tr><td> receive: ".	
				$result["receiver_key"]  .
				
				" </td></tr><tr><td> req: ".
                                $result["request"] .
		
				" </td></tr><tr><td> message: ".
                                $result["message"] .	
	
				"</td></tr></table><br>";
		}
	}
?>

