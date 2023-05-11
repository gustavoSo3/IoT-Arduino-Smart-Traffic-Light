import boto3
from datetime import datetime, timedelta

s3_client = boto3.client('s3')
rekognition_client = boto3.client('rekognition')
dynamodb = boto3.resource('dynamodb')
table_name = "processed_image_data"
table = dynamodb.Table(table_name)

def lambda_handler(event, context):
    bucket_name = event['Records'][0]['s3']['bucket']['name']
    image_key = event['Records'][0]['s3']['object']['key']

    response = rekognition_client.detect_labels(
        Image={
            'S3Object': {
                'Bucket': bucket_name,
                'Name': image_key,
            }
        },
        MaxLabels=20,
        MinConfidence=90
    )

    label_counts = {}
    for label in response['Labels']:
        name = label['Name']
        label_counts[name] = label_counts.get(name, 0) + 1

    current_time = datetime.now()
    timestamp = current_time.strftime('%Y-%m-%d %H:%M:%S')
    item = {
        'ImageName': image_key,
        'LabelCounts': label_counts,
        'Timestamp': timestamp
    }

    try:
        table.put_item(Item=item, ConditionExpression='attribute_not_exists(ImageName)')
        print(f"Item {item} added to DynamoDB table {table_name}")
    except Exception as e:
        print(f"Error adding item {item} to DynamoDB table {table_name}: {e}")