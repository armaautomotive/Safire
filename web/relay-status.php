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

                        echo " <table><tr><td>". 
                                $result["type"]  .        
                                " </td><td>".
                                
                                $result["sender_key"]  .        
                                " </td><td>".   
                                $result["receiver_key"]  .
                                
                                " </td><td>".
                                $result["request"] .
                
                                " </td><td>".
                                $result["message"] .    
        
                                "</td></tr></table><br>";
                }
        }
?>
