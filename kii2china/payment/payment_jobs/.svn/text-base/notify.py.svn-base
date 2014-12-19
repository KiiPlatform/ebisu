import smtplib
from email.mime.text import MIMEText

def send_notification(toaddrs, sub, body):

    fromaddr = 'kii.test.001@gmail.com'

    msg = MIMEText(body)

    msg['Subject'] = sub
    msg['From'] = fromaddr
    msg['To'] =  ', ' . join(toaddrs)


    # Credentials (if needed)  
    username = fromaddr
    password = 'wa5ikgkm'  
  
    # The actual mail send  
    server = smtplib.SMTP('smtp.gmail.com:587')  
    server.starttls()  
    server.login(username,password)  
    server.sendmail(fromaddr, toaddrs, msg.as_string())  
    server.quit()
