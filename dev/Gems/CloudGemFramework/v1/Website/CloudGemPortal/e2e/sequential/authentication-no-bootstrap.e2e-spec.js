var session = require('../session-helper.js');

var num_messages = 7;

/**
 * Message of the day integration tests
 * For this to run correctly delete all your messages from Active and Planned sections
 */
describe('Authentication without a bootstrap', function () {    
    var page = {
        header: $('div.modal-header h2'),        
        submit: $('#modal-dismiss')
    }

    beforeAll(function () {
        browser.get(browser.params.url);
        console.log("Waiting for the Cloud Gem Portal to load.")
        browser.wait(until.urlContains(session.loginPath), 10000, "urlContains"); // Checks that the current URL contains the expected text           
    }); 

    describe('integration tests', function () {        
        it('should display a informational bootstrap modal', function () {            
            browser.wait(until.presenceOf(page.header), 5000, "Modal header")                                
            expect(page.header.getText()).toEqual("Missing Cloud Gem Portal bootstrap information.")            
            page.submit.click();                     
        });
    });
});