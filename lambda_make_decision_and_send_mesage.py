import boto3
import json
import paho.mqtt.publish as publish
from datetime import datetime

# Initialize DynamoDB and MQTT clients
dynamodb = boto3.client('dynamodb')

# Define the MQTT topic to publish to
mqtt_topic = 'traffic_light_control'

# Define the MQTT server parameters
mqtt_host = 'ec2-18-234-62-133.compute-1.amazonaws.com'
mqtt_user = 'iot'
mqtt_password = '1234'

# Define the name of the DynamoDB table to store the decisions
decision_table_name = 'final_decision_iot_traffic'

def lambda_handler(event, context):
    for record in event['Records']:
        if record['eventName'] == 'INSERT':
            print("Record: ", record)
            # Extract the necessary information from the inserted document
            new_image = record['dynamodb']['NewImage']
            image_name = new_image['ImageName']['S']
            label_counts = new_image['LabelCounts']['M']
            timestamp = new_image['Timestamp']['S']
            
            # Make the decision based on the label counts
            if "Person" in label_counts:
                decision = "pedestrian"
            elif "Car" in label_counts:
                decision = "cars"
            else:
                decision = "unknown"
                
            # Send the decision through MQTT
            publish.single(
                topic = mqtt_topic,
                payload=decision,
                qos=1,
                hostname=mqtt_host,
                auth = {
                    "username":mqtt_user,
                    "password" : mqtt_password
                    
                }
            )
            
            # Store the decision in another DynamoDB table
            decision_item = {
                'ImageName': {'S': image_name},
                'Decision': {'S': decision},
                'Timestamp': {'S': datetime.now().strftime("%Y_%m_%d_%H_%M_%S")}
            }
            dynamodb.put_item(TableName=decision_table_name, Item=decision_item)
