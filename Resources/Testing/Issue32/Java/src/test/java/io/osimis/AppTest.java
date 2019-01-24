package io.osimis;

import java.io.IOException;
import java.util.Base64;
import org.apache.http.HttpEntity;
import org.apache.http.client.HttpRequestRetryHandler;
import org.apache.http.client.methods.CloseableHttpResponse;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.client.methods.HttpRequestBase;
import org.apache.http.impl.client.CloseableHttpClient;
import org.apache.http.impl.client.HttpClients;
import org.apache.http.impl.conn.PoolingHttpClientConnectionManager;
import org.apache.http.util.EntityUtils;

import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class AppTest 
{
  @Test
  public void testKeepAlive()
  {
    PoolingHttpClientConnectionManager cm = new PoolingHttpClientConnectionManager();

    CloseableHttpClient client = HttpClients
      .custom()
      .setConnectionManager(cm)
      .setRetryHandler((HttpRequestRetryHandler) (exception, executionCount, context) -> {
          System.out.println("ERROR");
          assertTrue(false);
          return false;
        }).build();

    HttpRequestBase request = new HttpGet("http://localhost:8042/system");

    // Low-level handling of HTTP basic authentication (for integration tests)
    request.addHeader("Authorization", "Basic " +
                      Base64.getEncoder().encodeToString("alice:orthanctest".getBytes()));

    // The following call works
    //HttpRequestBase request = new HttpGet("https://api.ipify.org?format=json");
    
    for (int i = 0; i < 5; i++) {
      System.out.println("================================");
      try (CloseableHttpResponse httpResponse = client.execute(request)) {
        String responseContent = null;

        HttpEntity entity = httpResponse.getEntity();
        if (entity != null) {
          responseContent = EntityUtils.toString(entity);
        }

        System.out.println(httpResponse.getStatusLine().getStatusCode());
        System.out.println(responseContent);

        EntityUtils.consume(entity);
        httpResponse.close();
      } catch (IOException e) {
        System.out.println("Request error " + e);
      }
    }
      
    assertTrue(true);
  }
}
