require 'zmq'

class GstPlayDaemon
  def initialize(address)
    ctx = ZMQ::Context.new(1)
    @rep = ctx.socket(ZMQ::REQ)
    @rep.connect address

    @rep.send "PUBSUB "
    pubsub_addr = parse_response(@rep.recv)

    puts "PubSub addr is #{pubsub_addr}"
    @pubsub = ctx.socket(ZMQ::SUB)
    @pubsub.connect pubsub_addr
  end

  def ping
    msg = "GstPlayDaemon"
    @rep.send "PING #{msg}"

    return @rep.recv().end_with?(msg)
  end

  def close
    @pubsub.close; @pubsub = nil
    @rep.close; @rep = nil
  end

  def tags(uri)
    @rep.send "TAGS #{uri}"
    response_to_tag_dict(@rep.recv)
  end

private

  def parse_response(msg)
    re = /^([A-Z]+)[ ]?(.*)$/
    m = re.match msg

    throw m[2] unless m[1] == "OK"
    m[2]
  end

  def response_to_tag_dict(msg)
    parse_response msg

    re = /^(.*)_([0-9]+)$/  ## someparam_0
    msg.lines.drop(1).each_slice(2).inject({}) do |acc,x| 
      key,value = x
      m = re.match key

      acc[m[1]] ||= []
      acc[m[1]] << value.chomp
      acc
    end
  end
end
