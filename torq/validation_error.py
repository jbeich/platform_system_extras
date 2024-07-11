class ValidationError:
  def __init__(self, message, suggestion):
    self.message = message
    self.suggestion = suggestion