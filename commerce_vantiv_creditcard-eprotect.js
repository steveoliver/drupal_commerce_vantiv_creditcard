/**
 * @file
 * Javascript to generate eProtect token in PCI-compliant way.
 */

(function ($) {
  console.log('loaded vantiv eprotect js!');
  Drupal.behaviors.vantivCreditCardEprotect = {
    attach: function (context, settings) {
      console.log('attaching vantivCreditCardEprotect!');
      var callLitle = function() {
        if (typeof LitlePayPage !== 'function') {
          alert("We are experiencing technical difficulties. Please try again later or call 555-555-1212 (API unavailable)" );
          return false;
        }
        return true;
      };
      if (settings.eprotectfetched == null) {
        settings.eprotectfetched = true;
        
        var vantivSettings = settings.vantiv;

        var submitButtonID = vantivSettings.checkout_pane ? '#commerce-checkout-form-review #edit-continue' : '#commerce-payment-order-transaction-add-form--2 #edit-submit';
        if (vantivSettings.cardonfile_form) {
          submitButtonID = '#commerce-vantiv-creditcard-cardonfile-create-form #edit-submit';
        }

        $('body').delegate(submitButtonID, 'click', function(event) {
          if (!callLitle()) {
            return false;
          }
          var form$ = $('#' + event.currentTarget.id).closest("form");
          var setLitleResponseFields = function(response) {
            document.getElementById('response$code').value = response.response || '';
            document.getElementById('response$message').value = response.message || '';
            document.getElementById('response$responseTime').value = response.responseTime || '';
            document.getElementById('response$litleTxnId').value = response.litleTxnId || '';
            document.getElementById('response$type').value = response.type || '';
            document.getElementById('response$firstSix').value = response.firstSix || '';
            document.getElementById('response$lastFour').value = response.lastFour || '';
          };
          setLitleResponseFields({"response": "", "message": ""});
          // For regular checkout
          if (vantivSettings.checkout_pane) {
            var formFields = {
              "accountNum": document.getElementById("edit-commerce-payment-payment-details-credit-card-number"),
              "cvv2": document.getElementById("edit-commerce-payment-payment-details-credit-card-code"),
              "paypageRegistrationId": document.getElementById("response$paypageRegistrationId"),
              "bin": document.getElementById("response$bin")
            };
          }
          else if (vantivSettings.payment_pane) {
            // For Add Payment form
            var formFields = {
              "accountNum": document.getElementById("edit-payment-details-credit-card-number"),
              "cvv2": document.getElementById("edit-payment-details-credit-card-code"),
              "paypageRegistrationId": document.getElementById("response$paypageRegistrationId"),
              "bin": document.getElementById("response$bin")
            };
          }
          else if (vantivSettings.cardonfile_form) {
            var formFields = {
              "accountNum": document.getElementById("edit-credit-card-number"),
              "cvv2": document.getElementById("edit-credit-card-code"),
              "paypageRegistrationId": document.getElementById("response$paypageRegistrationId"),
              "bin": document.getElementById("response$bin")
            };
          }
          var submitAfterLitle = function(response) {
            setLitleResponseFields(response);
            form$.get(0).submit(form$);
          };
          var onErrorAfterLitle = function(response) {
            setLitleResponseFields(response);
            if (response.response == '871') {
              alert("Invalid card number. Check and retry. (Not Mod10)");
            }
            else if (response.response == '872') {
              alert("Invalid card number. Check and retry. (Too short)");
            }
            else if (response.response == '873') {
              alert("Invalid card number. Check and retry. (Too long)");
            }
            else if (response.response == '874') {
              alert("Invalid card number. Check and retry. (Not a number)");
            }
            else if (response.response == '875') {
              alert("We are experiencing technical difficulties. Please try again later or call 555-555-1212");
            }
            else if (response.response == '876') {
              alert("Invalid card number. Check and retry. (Failure from Server)");
            }
            else if (response.response == '881') {
              alert("Invalid card validation code. Check and retry. (Not a number)");
            }
            else if (response.response == '882') {
              alert("Invalid card validation code. Check and retry. (Too short)");
            }
            else if (response.response == '883') {
              alert("Invalid card validation code. Check and retry. (Too long)");
            }
            else if (response.response == '889') {
              alert("We are experiencing technical difficulties. Please try again later or call 555-555-1212");
            }
            return false;
          };
          var timeoutOnLitle = function() {
            alert("We are experiencing technical difficulties. Please try again later call 555-555-1212 (timeout)");
          };
          var timeout = 15000;
          var litleRequest = {
            "paypageId": document.getElementById("request$paypageId").value,
            "reportGroup": document.getElementById("request$reportGroup").value,
            "orderId": document.getElementById("request$orderId").value,
            "id": document.getElementById("request$merchantTxnId").value,
            "url": "https://request-prelive.np-securepaypage-litle.com"
          };
          new LitlePayPage().sendToLitle(litleRequest, formFields, submitAfterLitle, onErrorAfterLitle, timeoutOnLitle, timeout);
          return false;
        });
      }
    }
  }
})(jQuery);
