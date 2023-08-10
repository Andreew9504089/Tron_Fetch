using RabbitMQ.Client;
using RabbitMQ.Client.Events;
using System.Text;
using System.Text.Json;

class Example
{
    public static void Main(string[] args){
        Console.WriteLine("The current time is " + DateTime.Now);
        string amqpHost = "nycu-test.pub.tft.tw"; // 192.168.140.1 (⻄⼦灣測驗環境) 
        string amqpUserName = "guest";
        string amqpPassword = "guest";
        int amqpPort = 5672;

        string exchangeName = "t-meta-edge";
        string routingKeyName = "api.event.target.*";
        var factory = new ConnectionFactory(){
            UserName = amqpUserName,
            Password = amqpPassword,
            HostName = amqpHost,
            Port = amqpPort
        };
        using(var connection = factory.CreateConnection()) // connect to rabbit mq
        using(var channel = connection.CreateModel()){ // create channel
            string queueName = channel.QueueDeclare().QueueName; // declare queue and get queue name
            channel.QueueBind(queue: queueName,
            exchange: exchangeName,
            routingKey: routingKeyName);
            EventingBasicConsumer consumer = new EventingBasicConsumer(channel);
            consumer.Received += (model, ea) =>{
                var body = ea.Body.ToArray();
                var message = Encoding.UTF8.GetString(body);
                Console.WriteLine(message); // print one line for each message
            };
            channel.BasicConsume(queue: queueName,
            autoAck: true,
            consumer: consumer);
            Console.ReadLine(); // press enter to exit
        }
    }
}
