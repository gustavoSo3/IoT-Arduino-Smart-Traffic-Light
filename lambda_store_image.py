import boto3
import base64
import json
from datetime import datetime, timedelta


s3 = boto3.client('s3')

def lambda_handler(event, context):
    # Get the Base64-encoded image data from the request
    image_json = event['body']
    image_object = json.loads(image_json)
    image_data = image_object["image"]
    
    # Decode the Base64-encoded image data
    image = base64.b64decode(image_data)
    #name of the file includes the time
    current_time = datetime.now()
    timestamp = current_time.strftime('%Y_%m_%d_%H_%M_%S')
    # Store the image in an S3 bucket
    s3.put_object(
        Body=image,
        Bucket='trafficrawimages',
        Key= timestamp + '.jpg'
    )
    
    # Return a success response
    return {
        'statusCode': 200,
        'body': 'Image stored successfully'
    }
