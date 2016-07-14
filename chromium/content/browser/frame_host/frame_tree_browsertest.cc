// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "build/build_config.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/test_frame_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"
#include "url/url_constants.h"

// For fine-grained suppression on flaky tests.
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace content {

class FrameTreeBrowserTest : public ContentBrowserTest {
 public:
  FrameTreeBrowserTest() {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    SetupCrossSiteRedirector(embedded_test_server());
  }

 protected:
  std::string GetOriginFromRenderer(FrameTreeNode* node) {
    std::string origin;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        node->current_frame_host(),
        "window.domAutomationController.send(document.origin);", &origin));
    return origin;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameTreeBrowserTest);
};

// Ensures FrameTree correctly reflects page structure during navigations.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, FrameTreeShape) {
  GURL base_url = embedded_test_server()->GetURL("A.com", "/site_isolation/");

  // Load doc without iframes. Verify FrameTree just has root.
  // Frame tree:
  //   Site-A Root
  NavigateToURL(shell(), base_url.Resolve("blank.html"));
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(shell()->web_contents())->
      GetFrameTree()->root();
  EXPECT_EQ(0U, root->child_count());

  // Add 2 same-site frames. Verify 3 nodes in tree with proper names.
  // Frame tree:
  //   Site-A Root -- Site-A frame1
  //              \-- Site-A frame2
  WindowedNotificationObserver observer1(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &shell()->web_contents()->GetController()));
  NavigateToURL(shell(), base_url.Resolve("frames-X-X.html"));
  observer1.Wait();
  ASSERT_EQ(2U, root->child_count());
  EXPECT_EQ(0U, root->child_at(0)->child_count());
  EXPECT_EQ(0U, root->child_at(1)->child_count());
}

// TODO(ajwong): Talk with nasko and merge this functionality with
// FrameTreeShape.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, FrameTreeShape2) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_tree/top.html"));

  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetFrameTree()->root();

  // Check that the root node is properly created.
  ASSERT_EQ(3UL, root->child_count());
  EXPECT_EQ(std::string(), root->frame_name());

  ASSERT_EQ(2UL, root->child_at(0)->child_count());
  EXPECT_STREQ("1-1-name", root->child_at(0)->frame_name().c_str());

  // Verify the deepest node exists and has the right name.
  ASSERT_EQ(2UL, root->child_at(2)->child_count());
  EXPECT_EQ(1UL, root->child_at(2)->child_at(1)->child_count());
  EXPECT_EQ(0UL, root->child_at(2)->child_at(1)->child_at(0)->child_count());
  EXPECT_STREQ("3-1-name",
      root->child_at(2)->child_at(1)->child_at(0)->frame_name().c_str());

  // Navigate to about:blank, which should leave only the root node of the frame
  // tree in the browser process.
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  root = wc->GetFrameTree()->root();
  EXPECT_EQ(0UL, root->child_count());
  EXPECT_EQ(std::string(), root->frame_name());
}

// Test that we can navigate away if the previous renderer doesn't clean up its
// child frames.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, FrameTreeAfterCrash) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_tree/top.html"));

  // Ensure the view and frame are live.
  RenderViewHost* rvh = shell()->web_contents()->GetRenderViewHost();
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(rvh->GetMainFrame());
  EXPECT_TRUE(rvh->IsRenderViewLive());
  EXPECT_TRUE(rfh->IsRenderFrameLive());

  // Crash the renderer so that it doesn't send any FrameDetached messages.
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  NavigateToURL(shell(), GURL(kChromeUICrashURL));
  crash_observer.Wait();

  // The frame tree should be cleared.
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetFrameTree()->root();
  EXPECT_EQ(0UL, root->child_count());

  // Ensure the view and frame aren't live anymore.
  EXPECT_FALSE(rvh->IsRenderViewLive());
  EXPECT_FALSE(rfh->IsRenderFrameLive());

  // Navigate to a new URL.
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  NavigateToURL(shell(), url);
  EXPECT_EQ(0UL, root->child_count());
  EXPECT_EQ(url, root->current_url());

  // Ensure the view and frame are live again.
  EXPECT_TRUE(rvh->IsRenderViewLive());
  EXPECT_TRUE(rfh->IsRenderFrameLive());
}

// Test that we can navigate away if the previous renderer doesn't clean up its
// child frames.
// Flaky on Mac. http://crbug.com/452018
#if defined(OS_MACOSX)
#define MAYBE_NavigateWithLeftoverFrames DISABLED_NavigateWithLeftoverFrames
#else
#define MAYBE_NavigateWithLeftoverFrames NavigateWithLeftoverFrames
#endif
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, MAYBE_NavigateWithLeftoverFrames) {
#if defined(OS_WIN)
  // Flaky on XP bot http://crbug.com/468713
  if (base::win::GetVersion() <= base::win::VERSION_XP)
    return;
#endif
  GURL base_url = embedded_test_server()->GetURL("A.com", "/site_isolation/");

  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/frame_tree/top.html"));

  // Hang the renderer so that it doesn't send any FrameDetached messages.
  // (This navigation will never complete, so don't wait for it.)
  shell()->LoadURL(GURL(kChromeUIHangURL));

  // Check that the frame tree still has children.
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetFrameTree()->root();
  ASSERT_EQ(3UL, root->child_count());

  // Navigate to a new URL.  We use LoadURL because NavigateToURL will try to
  // wait for the previous navigation to stop.
  TestNavigationObserver tab_observer(wc, 1);
  shell()->LoadURL(base_url.Resolve("blank.html"));
  tab_observer.Wait();

  // The frame tree should now be cleared.
  EXPECT_EQ(0UL, root->child_count());
}

// Ensure that IsRenderFrameLive is true for main frames and same-site iframes.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, IsRenderFrameLive) {
  GURL main_url(embedded_test_server()->GetURL("/frame_tree/top.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()->root();

  // The root and subframe should each have a live RenderFrame.
  EXPECT_TRUE(
      root->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());

  // Load a same-site page into iframe and it should still be live.
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(root->child_at(0), http_url);
  EXPECT_TRUE(
      root->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());
}

// Ensure that origins are correctly set on navigations.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, OriginSetOnNavigation) {
  GURL about_blank(url::kAboutBlankURL);
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContents* contents = shell()->web_contents();

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetFrameTree()->root();

  // Extra '/' is added because the replicated origin is serialized in RFC 6454
  // format, which dictates no trailing '/', whereas GURL::GetOrigin does put a
  // '/' at the end.
  EXPECT_EQ(main_url.GetOrigin().spec(),
            root->current_origin().Serialize() + '/');
  EXPECT_EQ(
      main_url.GetOrigin().spec(),
      root->current_frame_host()->GetLastCommittedOrigin().Serialize() + '/');

  // The iframe is inititially same-origin.
  EXPECT_TRUE(
      root->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
          root->child_at(0)->current_frame_host()->GetLastCommittedOrigin()));
  EXPECT_EQ(root->current_origin().Serialize(), GetOriginFromRenderer(root));
  EXPECT_EQ(root->child_at(0)->current_origin().Serialize(),
            GetOriginFromRenderer(root->child_at(0)));

  // Navigate the iframe cross-origin.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);
  EXPECT_EQ(frame_url, root->child_at(0)->current_url());
  EXPECT_EQ(frame_url.GetOrigin().spec(),
            root->child_at(0)->current_origin().Serialize() + '/');
  EXPECT_FALSE(
      root->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
          root->child_at(0)->current_frame_host()->GetLastCommittedOrigin()));
  EXPECT_EQ(root->current_origin().Serialize(), GetOriginFromRenderer(root));
  EXPECT_EQ(root->child_at(0)->current_origin().Serialize(),
            GetOriginFromRenderer(root->child_at(0)));

  // Parent-initiated about:blank navigation should inherit the parent's a.com
  // origin.
  NavigateIframeToURL(contents, "1-1-id", about_blank);
  EXPECT_EQ(about_blank, root->child_at(0)->current_url());
  EXPECT_EQ(main_url.GetOrigin().spec(),
            root->child_at(0)->current_origin().Serialize() + '/');
  EXPECT_EQ(root->current_frame_host()->GetLastCommittedOrigin().Serialize(),
            root->child_at(0)
                ->current_frame_host()
                ->GetLastCommittedOrigin()
                .Serialize());
  EXPECT_TRUE(
      root->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
          root->child_at(0)->current_frame_host()->GetLastCommittedOrigin()));
  EXPECT_EQ(root->current_origin().Serialize(), GetOriginFromRenderer(root));
  EXPECT_EQ(root->child_at(0)->current_origin().Serialize(),
            GetOriginFromRenderer(root->child_at(0)));

  GURL data_url("data:text/html,foo");
  EXPECT_TRUE(NavigateToURL(shell(), data_url));

  // Navigating to a data URL should set a unique origin.  This is represented
  // as "null" per RFC 6454.
  EXPECT_EQ("null", root->current_origin().Serialize());
  EXPECT_TRUE(contents->GetMainFrame()->GetLastCommittedOrigin().unique());
  EXPECT_EQ("null", GetOriginFromRenderer(root));

  // Re-navigating to a normal URL should update the origin.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(main_url.GetOrigin().spec(),
            root->current_origin().Serialize() + '/');
  EXPECT_EQ(
      main_url.GetOrigin().spec(),
      contents->GetMainFrame()->GetLastCommittedOrigin().Serialize() + '/');
  EXPECT_FALSE(contents->GetMainFrame()->GetLastCommittedOrigin().unique());
  EXPECT_EQ(root->current_origin().Serialize(), GetOriginFromRenderer(root));
}

// Tests a cross-origin navigation to a blob URL. The main frame initiates this
// navigation on its grandchild. It should wind up in the main frame's process.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, NavigateGrandchildToBlob) {
  WebContents* contents = shell()->web_contents();
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetFrameTree()->root();

  // First, snapshot the FrameTree for a normal A(B(A)) case where all frames
  // are served over http. The blob test should result in the same structure.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b(a))")));
  std::string reference_tree = FrameTreeVisualizer().DepictFrameTree(root);

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // The root node will initiate the navigation; its grandchild node will be the
  // target of the navigation.
  FrameTreeNode* target = root->child_at(0)->child_at(0);

  std::string blob_url_string;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root->current_frame_host(),
      "function receiveMessage(event) {"
      "  document.body.appendChild(document.createTextNode(event.data));"
      "  domAutomationController.send(event.source.location.href);"
      "}"
      "window.addEventListener('message', receiveMessage, false);"
      "var blob = new Blob(["
      "    '<html><body><div>This is blob content.</div><script>"
      "         window.parent.parent.postMessage(\"HI\", document.origin);"
      "     </script></body></html>'], {type: 'text/html'});"
      "var blob_url = URL.createObjectURL(blob);"
      "frames[0][0].location.href = blob_url;",
      &blob_url_string));
  EXPECT_EQ(GURL(blob_url_string), target->current_url());
  EXPECT_EQ(url::kBlobScheme, target->current_url().scheme());
  EXPECT_FALSE(target->current_origin().unique());
  EXPECT_EQ("a.com", target->current_origin().host());
  EXPECT_EQ(url::kHttpScheme, target->current_origin().scheme());

  std::string document_body;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      target->current_frame_host(),
      "domAutomationController.send(document.body.children[0].innerHTML);",
      &document_body));
  EXPECT_EQ("This is blob content.", document_body);
  EXPECT_EQ(reference_tree, FrameTreeVisualizer().DepictFrameTree(root));
}

IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, NavigateChildToAboutBlank) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // The leaf node (c.com) will be navigated. Its parent node (b.com) will
  // initiate the navigation.
  FrameTreeNode* target =
      contents->GetFrameTree()->root()->child_at(0)->child_at(0);
  FrameTreeNode* initiator = target->parent();

  // Give the target a name.
  EXPECT_TRUE(
      ExecuteScript(target->current_frame_host(), "window.name = 'target';"));

  // Use window.open(about:blank), then poll the document for access.
  std::string about_blank_origin;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      initiator->current_frame_host(),
      "var didNavigate = false;"
      "var intervalID = setInterval(function() {"
      "  if (!didNavigate) {"
      "    didNavigate = true;"
      "    window.open('about:blank', 'target');"
      "  }"
      "  // Poll the document until it doesn't throw a SecurityError.\n"
      "  try {"
      "    frames[0].document.write('Hi from ' + document.domain);"
      "  } catch (e) { return; }"
      "  clearInterval(intervalID);"
      "  domAutomationController.send(frames[0].document.origin);"
      "}, 16);",
      &about_blank_origin));
  EXPECT_EQ(GURL(url::kAboutBlankURL), target->current_url());
  EXPECT_EQ(url::kAboutScheme, target->current_url().scheme());
  EXPECT_FALSE(target->current_origin().unique());
  EXPECT_EQ("b.com", target->current_origin().host());
  EXPECT_EQ(url::kHttpScheme, target->current_origin().scheme());
  EXPECT_EQ(target->current_origin().Serialize(), about_blank_origin);

  std::string document_body;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      target->current_frame_host(),
      "domAutomationController.send(document.body.innerHTML);",
      &document_body));
  EXPECT_EQ("Hi from b.com", document_body);
}

// Nested iframes, three origins: A(B(C)). Frame A navigates C to about:blank
// (via window.open). This should wind up in A's origin per the spec. Test fails
// because of http://crbug.com/564292
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest,
                       DISABLED_NavigateGrandchildToAboutBlank) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  // The leaf node (c.com) will be navigated. Its grandparent node (a.com) will
  // initiate the navigation.
  FrameTreeNode* target =
      contents->GetFrameTree()->root()->child_at(0)->child_at(0);
  FrameTreeNode* initiator = target->parent()->parent();

  // Give the target a name.
  EXPECT_TRUE(
      ExecuteScript(target->current_frame_host(), "window.name = 'target';"));

  // Use window.open(about:blank), then poll the document for access.
  std::string about_blank_origin;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      initiator->current_frame_host(),
      "var didNavigate = false;"
      "var intervalID = setInterval(function() {"
      "  if (!didNavigate) {"
      "    didNavigate = true;"
      "    window.open('about:blank', 'target');"
      "  }"
      "  // May raise a SecurityError, that's expected.\n"
      "  frames[0][0].document.write('Hi from ' + document.domain);"
      "  clearInterval(intervalID);"
      "  domAutomationController.send(frames[0][0].document.origin);"
      "}, 16);",
      &about_blank_origin));
  EXPECT_EQ(GURL(url::kAboutBlankURL), target->current_url());
  EXPECT_EQ(url::kAboutScheme, target->current_url().scheme());
  EXPECT_FALSE(target->current_origin().unique());
  EXPECT_EQ("a.com", target->current_origin().host());
  EXPECT_EQ(url::kHttpScheme, target->current_origin().scheme());
  EXPECT_EQ(target->current_origin().Serialize(), about_blank_origin);

  std::string document_body;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      target->current_frame_host(),
      "domAutomationController.send(document.body.innerHTML);",
      &document_body));
  EXPECT_EQ("Hi from a.com", document_body);
}

// Ensures that iframe with srcdoc is always put in the same origin as its
// parent frame.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, ChildFrameWithSrcdoc) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  WebContentsImpl* contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = contents->GetFrameTree()->root();
  EXPECT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);
  std::string frame_origin;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      child->current_frame_host(),
      "domAutomationController.send(document.origin);", &frame_origin));
  EXPECT_TRUE(
      child->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
          url::Origin(GURL(frame_origin))));
  EXPECT_FALSE(
      root->current_frame_host()->GetLastCommittedOrigin().IsSameOriginWith(
          url::Origin(GURL(frame_origin))));

  // Create a new iframe with srcdoc and add it to the main frame. It should
  // be created in the same SiteInstance as the parent.
  {
    std::string script("var f = document.createElement('iframe');"
                       "f.srcdoc = 'some content';"
                       "document.body.appendChild(f)");
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(root->current_frame_host(), script));
    EXPECT_EQ(2U, root->child_count());
    observer.Wait();

    EXPECT_EQ(GURL(url::kAboutBlankURL), root->child_at(1)->current_url());
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        root->child_at(1)->current_frame_host(),
        "domAutomationController.send(document.origin);", &frame_origin));
    EXPECT_EQ(root->current_frame_host()->GetLastCommittedURL().GetOrigin(),
              GURL(frame_origin));
    EXPECT_NE(child->current_frame_host()->GetLastCommittedURL().GetOrigin(),
              GURL(frame_origin));
  }

  // Set srcdoc on the existing cross-site frame. It should navigate the frame
  // back to the origin of the parent.
  {
    std::string script("var f = document.getElementById('child-0');"
                       "f.srcdoc = 'some content';");
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(root->current_frame_host(), script));
    observer.Wait();

    EXPECT_EQ(GURL(url::kAboutBlankURL), child->current_url());
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        child->current_frame_host(),
        "domAutomationController.send(document.origin);", &frame_origin));
    EXPECT_EQ(root->current_frame_host()->GetLastCommittedURL().GetOrigin(),
              GURL(frame_origin));
  }
}

// Ensure that sandbox flags are correctly set when child frames are created.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, SandboxFlagsSetForChildFrames) {
  GURL main_url(embedded_test_server()->GetURL("/sandboxed_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()->root();

  // Verify that sandbox flags are set properly for all FrameTreeNodes.
  // First frame is completely sandboxed; second frame uses "allow-scripts",
  // which resets both SandboxFlags::Scripts and
  // SandboxFlags::AutomaticFeatures bits per blink::parseSandboxPolicy(), and
  // third frame has "allow-scripts allow-same-origin".
  EXPECT_EQ(blink::WebSandboxFlags::None, root->effective_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::All,
            root->child_at(0)->effective_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::All & ~blink::WebSandboxFlags::Scripts &
                ~blink::WebSandboxFlags::AutomaticFeatures,
            root->child_at(1)->effective_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::All & ~blink::WebSandboxFlags::Scripts &
                ~blink::WebSandboxFlags::AutomaticFeatures &
                ~blink::WebSandboxFlags::Origin,
            root->child_at(2)->effective_sandbox_flags());

  // Sandboxed frames should set a unique origin unless they have the
  // "allow-same-origin" directive.
  EXPECT_EQ("null", root->child_at(0)->current_origin().Serialize());
  EXPECT_EQ("null", root->child_at(1)->current_origin().Serialize());
  EXPECT_EQ(main_url.GetOrigin().spec(),
            root->child_at(2)->current_origin().Serialize() + "/");

  // Navigating to a different URL should not clear sandbox flags.
  GURL frame_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);
  EXPECT_EQ(blink::WebSandboxFlags::All,
            root->child_at(0)->effective_sandbox_flags());
}

// Ensure that a popup opened from a subframe sets its opener to the subframe's
// FrameTreeNode, and that the opener is cleared if the subframe is destroyed.
IN_PROC_BROWSER_TEST_F(FrameTreeBrowserTest, SubframeOpenerSetForNewWindow) {
  GURL main_url(embedded_test_server()->GetURL("/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Open a new window from a subframe.
  ShellAddedObserver new_shell_observer;
  GURL popup_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(ExecuteScript(root->child_at(0)->current_frame_host(),
                            "window.open('" + popup_url.spec() + "');"));
  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* new_contents = new_shell->web_contents();
  WaitForLoadStop(new_contents);

  // Check that the new window's opener points to the correct subframe on
  // original window.
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_contents)->GetFrameTree()->root();
  EXPECT_EQ(root->child_at(0), popup_root->opener());

  // Close the original window.  This should clear the new window's opener.
  shell()->Close();
  EXPECT_EQ(nullptr, popup_root->opener());
}

class CrossProcessFrameTreeBrowserTest : public ContentBrowserTest {
 public:
  CrossProcessFrameTreeBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    SetupCrossSiteRedirector(embedded_test_server());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrossProcessFrameTreeBrowserTest);
};

// Ensure that we can complete a cross-process subframe navigation.
IN_PROC_BROWSER_TEST_F(CrossProcessFrameTreeBrowserTest,
                       CreateCrossProcessSubframeProxies) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()->root();

  // There should not be a proxy for the root's own SiteInstance.
  SiteInstance* root_instance = root->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(root->render_manager()->GetRenderFrameProxyHost(root_instance));

  // Load same-site page into iframe.
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(root->child_at(0), http_url);

  // Load cross-site page into iframe.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  // Ensure that we have created a new process for the subframe.
  ASSERT_EQ(2U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  SiteInstance* child_instance = child->current_frame_host()->GetSiteInstance();
  RenderViewHost* rvh = child->current_frame_host()->render_view_host();
  RenderProcessHost* rph = child->current_frame_host()->GetProcess();

  EXPECT_NE(shell()->web_contents()->GetRenderViewHost(), rvh);
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(), child_instance);
  EXPECT_NE(shell()->web_contents()->GetRenderProcessHost(), rph);

  // Ensure that the root node has a proxy for the child node's SiteInstance.
  EXPECT_TRUE(root->render_manager()->GetRenderFrameProxyHost(child_instance));

  // Also ensure that the child has a proxy for the root node's SiteInstance.
  EXPECT_TRUE(child->render_manager()->GetRenderFrameProxyHost(root_instance));

  // The nodes should not have proxies for their own SiteInstance.
  EXPECT_FALSE(root->render_manager()->GetRenderFrameProxyHost(root_instance));
  EXPECT_FALSE(
      child->render_manager()->GetRenderFrameProxyHost(child_instance));

  // Ensure that the RenderViews and RenderFrames are all live.
  EXPECT_TRUE(
      root->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(
      child->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(root->child_at(0)->current_frame_host()->IsRenderFrameLive());
}

IN_PROC_BROWSER_TEST_F(CrossProcessFrameTreeBrowserTest,
                       OriginSetOnCrossProcessNavigations) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()->root();

  EXPECT_EQ(root->current_origin().Serialize() + '/',
            main_url.GetOrigin().spec());

  // First frame is an about:blank frame.  Check that its origin is correctly
  // inherited from the parent.
  EXPECT_EQ(root->child_at(0)->current_origin().Serialize() + '/',
            main_url.GetOrigin().spec());

  // Second frame loads a same-site page.  Its origin should also be the same
  // as the parent.
  EXPECT_EQ(root->child_at(1)->current_origin().Serialize() + '/',
            main_url.GetOrigin().spec());

  // Load cross-site page into the first frame.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title2.html"));
  NavigateFrameToURL(root->child_at(0), cross_site_url);

  EXPECT_EQ(root->child_at(0)->current_origin().Serialize() + '/',
            cross_site_url.GetOrigin().spec());

  // The root's origin shouldn't have changed.
  EXPECT_EQ(root->current_origin().Serialize() + '/',
            main_url.GetOrigin().spec());

  GURL data_url("data:text/html,foo");
  NavigateFrameToURL(root->child_at(1), data_url);

  // Navigating to a data URL should set a unique origin.  This is represented
  // as "null" per RFC 6454.
  EXPECT_EQ(root->child_at(1)->current_origin().Serialize(), "null");
}

}  // namespace content
